"""
Deterministic, hand-designed algorithm test cases for TISP_limiter.

The 60 cases in test_cases_list.csv are a GEOMETRY matrix: numChannels x blockSize,
stressing SE/SA predication, vector tails and buffer pitches with a randomly drawn
sine. They exercise every branch of the limiter somewhere in the suite, but nothing
in them asserts a specific algorithmic behaviour, and their parameters are drawn
from ranges that keep the envelope permanently settled.

The cases here are the complementary ALGORITHM test: no RNG, coefficients that are
physically meaningful, and signal geometry chosen so that one wrong term produces a
visibly wrong vector rather than a small numerical drift.

    A (64) static probe   - isolates the static characteristic and the make-up gain
    B (65) amplitude sweep- attack/release asymmetry over the full dynamic range
    C (66) burst          - the paper's Fig. 3/5/8 envelope transient
    D (67) AM sine        - paper Eq. (24), hard knee

Case ids continue the csv's sequence, after the geometry matrix and the bypass cases.
Because that makes them positional, gen_test_cases_list.py asserts the ids here are exactly
the next contiguous run - so adding a geometry or bypass case fails loudly instead of
silently colliding.

Notes that constrain these designs:

  * staticIn is emitted with "{:.6f}", which puts a quantization floor near -60 dBFS
    (0.0043 dB of error at -60 dB, 0.43 dB at -100 dB). Quiet segments are therefore
    designed at -60 dBFS, and true silence is expressed as an exact 0.0 rather than
    approached with a very small number.

  * Exact-0.0 samples appear ONLY in case A, which runs with attackCoeff =
    releaseCoeff = 0. Because dB20 maps x <= 0 to a large constant, a zero sample
    perturbs the envelope; with zero coefficients the envelope has no memory, so the
    perturbation stays local to that sample instead of propagating through the rest
    of the block and flattening the whole vector.

  * Levels avoid sitting exactly on a knee breakpoint. staticIn is rounded to 6
    decimals, which nudges a level by a few 1e-6 dB, and the region tests are strict
    < / >, so a probe placed exactly on a breakpoint could land on either side.
    Every level below is at least 0.5 dB clear of every channel's breakpoints.
"""

import numpy as np

# Must match the format string used for staticIn / staticOut in limiter.py.
SAMPLE_DECIMALS = 6


def _lin(dbfs):
    """dBFS -> linear amplitude."""
    return 10.0 ** (dbfs / 20.0)


def _quantize(x):
    """Round to what the emitted C header can actually represent."""
    return np.round(x, SAMPLE_DECIMALS)


def _carrier(blockSize, cycles, kind=np.sin):
    """A carrier that never lands exactly on a zero crossing.

    A plain sin(2*pi*f*n/N) with an integer number of cycles hits exact zeros at
    sample boundaries, and anything below 5e-7 quantizes to exactly 0.0 at 6
    decimals. That matters far more than it looks: dB20 maps x <= 0 to a large
    constant, which drives the envelope to a huge negative gain that a realistic
    release coefficient cannot recover from inside one block - the rest of the
    vector then reads as silence and the case stops testing what it was built to
    test. Offsetting the phase by half a sample step puts every sample at least
    sin(pi/samplesPerCycle) away from a zero crossing.
    """
    n = np.arange(blockSize)
    phaseStep = 2 * np.pi * cycles / blockSize
    return kind(phaseStep * n + phaseStep / 2)


def _assert_no_silence(testId, x):
    """Guard: only case A is allowed to contain exact-0.0 samples."""
    zeros = int(np.sum(x == 0.0))
    if zeros:
        raise ValueError(
            f"designed case {testId}: {zeros} sample(s) quantize to exactly 0.0. "
            f"Only the static probe (alpha = 0) may contain silence; elsewhere a zero "
            f"sample poisons the envelope for the rest of the block. Adjust the carrier "
            f"frequency or phase."
        )
    return x


def _params(channels):
    """Split a list of (threshold, knee, attack, release) tuples into arrays."""
    arr = np.array(channels, dtype=np.float64)
    return arr[:, 0].copy(), arr[:, 1].copy(), arr[:, 2].copy(), arr[:, 3].copy()


def _check(testId, expected, actual):
    if expected != actual:
        raise ValueError(
            f"designed case {testId}: expected (numChannels, blockSize) {expected}, "
            f"got {actual}. Regenerate test_cases_list.csv with gen_test_cases_list.py."
        )


# ---------------------------------------------------------------------------
# A (64): static probe
#
# attackCoeff = releaseCoeff = 0 collapses both smoothing branches to
# smoothedGain == targetGain, so every sample is an independent probe of the
# static characteristic with no envelope memory at all. That isolates the gain
# computer and the make-up gain from the level detector entirely.
#
# The invariant this case pins down: for EVERY channel, the 0 dBFS level must
# come out at exactly 1.0, because automatic make-up gain is defined as
# M = -x_sc(0 dB) and therefore x_sc(0 dB) + M == 0 by construction. That is
# MATLAB's MakeUpGainMode = 'Auto' contract, and it is the assertion that a
# per-sample make-up gain formula cannot satisfy.
# ---------------------------------------------------------------------------

# None means true silence (exact 0.0), not "very quiet".
_PROBE_LEVELS_DBFS = [
    None, -40.0, -30.0, -20.0, -14.0, -12.0, -10.0, -8.0,
    -7.0, -6.0, -5.0, -4.0, -2.0, -1.0, -0.5, 0.0,
]

_PROBE_HOLD = 8

_PROBE_CHANNELS = [
    # threshold, knee, attack, release
    (-6.0, 6.0, 0.0, 0.0),    # soft knee (-9 .. -3); 0 dBFS above knee end -> M = +6
    (-6.0, 0.0, 0.0, 0.0),    # hard knee                                   -> M = +6
    (-20.0, 15.0, 0.0, 0.0),  # wide soft knee (-27.5 .. -12.5)             -> M = +20
    (-3.0, 12.0, 0.0, 0.0),   # soft knee (-9 .. +3); 0 dBFS INSIDE the knee
    #                           -> M = +3.375, the only branch of the
    #                              make-up-gain computation the other three miss
]


def build_static_probe(testId, numChannels, blockSize):
    _check(testId, (len(_PROBE_CHANNELS), len(_PROBE_LEVELS_DBFS) * _PROBE_HOLD),
           (numChannels, blockSize))

    x = np.zeros((numChannels, blockSize))
    for li, level in enumerate(_PROBE_LEVELS_DBFS):
        amplitude = 0.0 if level is None else _lin(level)
        segment = np.full(_PROBE_HOLD, amplitude)

        # Alternate the sign so the segment is not DC: the detector folds sign away
        # with abs(), and a signed input proves it.
        if amplitude != 0.0:
            segment[1::2] *= -1.0

        x[:, li * _PROBE_HOLD:(li + 1) * _PROBE_HOLD] = segment

    return (_quantize(x),) + _params(_PROBE_CHANNELS)


# ---------------------------------------------------------------------------
# B (65): dB-linear amplitude sweep, -60 -> +6 -> -60 dBFS
#
# A FREQUENCY sweep would prove nothing here: the design is frequency-blind
# (peak detection on |x|, no sidechain filter, no lookahead). An AMPLITUDE
# sweep is the one that earns its place, and it is linear in dB because a
# linear ramp spends most of its samples in the top 6 dB.
#
# The two channels carry the same signal with attack/release SWAPPED. Equal or
# near-equal coefficients hide a swapped-branch bug; this pair cannot.
# ---------------------------------------------------------------------------

_SWEEP_CHANNELS = [
    (-6.0, 6.0, 0.95, 0.999),   # fast attack, slow release (normal)
    (-6.0, 6.0, 0.999, 0.95),   # deliberately mirrored
]

_SWEEP_CARRIER_CYCLES = 64.0


def build_amp_sweep(testId, numChannels, blockSize):
    _check(testId, (len(_SWEEP_CHANNELS), 2048), (numChannels, blockSize))

    half = blockSize // 2
    envelope_dBFS = np.concatenate([
        np.linspace(-60.0, 6.0, half, endpoint=False),
        np.linspace(6.0, -60.0, blockSize - half, endpoint=False),
    ])

    carrier = _carrier(blockSize, _SWEEP_CARRIER_CYCLES)
    x = np.tile(_lin(envelope_dBFS) * carrier, (numChannels, 1))

    return (_assert_no_silence(testId, _quantize(x)),) + _params(_SWEEP_CHANNELS)


# ---------------------------------------------------------------------------
# C (66): burst, -60 dBFS -> 0 dBFS -> -60 dBFS held
#
# The paper's Fig. 3 / 5 / 8 envelope test: a step onto the threshold and off
# again, with realistic asymmetric coefficients. releaseCoeff = 0.999 needs
# 4603 samples to reach 99% of a step, so the release deliberately does NOT
# complete inside this 2048-sample block. That is the point: the vector then
# describes the limiter mid-transient, which is a stronger comparison against
# the kernel than a settled steady state.
#
# No exact zeros here - see the module docstring.
# ---------------------------------------------------------------------------

_BURST_CHANNELS = [
    (-6.0, 6.0, 0.99, 0.999),    # soft knee
    (-12.0, 0.0, 0.99, 0.999),   # hard knee, deeper threshold
]

_BURST_QUIET_DBFS = -60.0
_BURST_LOUD_DBFS = 0.0
_BURST_CARRIER_CYCLES = 64.0


def build_burst(testId, numChannels, blockSize):
    _check(testId, (len(_BURST_CHANNELS), 2048), (numChannels, blockSize))

    lead, loud = 256, 1024
    envelope = np.full(blockSize, _lin(_BURST_QUIET_DBFS))
    envelope[lead:lead + loud] = _lin(_BURST_LOUD_DBFS)

    carrier = _carrier(blockSize, _BURST_CARRIER_CYCLES)
    x = np.tile(envelope * carrier, (numChannels, 1))

    return (_assert_no_silence(testId, _quantize(x)),) + _params(_BURST_CHANNELS)


# ---------------------------------------------------------------------------
# D (67): amplitude-modulated sine, paper Eq. (24), hard knee
#
#     x[n] = (1 + m*cos(2*pi*fm*n)) * cos(2*pi*fc*n)
#
# This is the paper's own signal for measuring effective compression ratio
# (Section 5, Fig. 10). Both channels use kneeWidth = 0, making this the
# deterministic home for the hard-knee path, which in the random geometry
# matrix depends on a knee width that rounds to zero.
#
# Scaled so the peak is exactly 0 dBFS: the paper's unscaled form peaks at
# (1 + m), and there is no reason to drive a limiter above full scale here.
# ---------------------------------------------------------------------------

_AM_CHANNELS = [
    (-6.0, 0.0, 0.95, 0.99),
    (-12.0, 0.0, 0.95, 0.99),
]

_AM_MOD_DEPTH = 0.5        # m
_AM_MOD_CYCLES = 2.0       # fm, cycles per block
_AM_CARRIER_CYCLES = 128.0  # fc, cycles per block

# fm and the coefficients are coupled, and getting it wrong makes this case inert:
# the envelope can only follow the modulation if a modulation HALF-period exceeds the
# smoothing settling time. At the first-attempt values (fm = 8, 0.98/0.995) the
# half-period was 64 samples against n99 = 228/919, so the gain travelled just 1.19 dB
# and the output modulation depth was not reduced at all (-0.14 dB) - the compressor
# was measured as doing nothing, which is not what the paper's Eq. (24) signal is for.
# At fm = 2 with 0.95/0.99 (n99 = 90/458, half-period 512) the gain travels 4.01 dB and
# the modulation depth drops 2.73 dB, so an effective compression ratio is actually
# observable. Re-measure if either is changed.


def build_am_sine(testId, numChannels, blockSize):
    _check(testId, (len(_AM_CHANNELS), 1024), (numChannels, blockSize))

    modulation = 1.0 + _AM_MOD_DEPTH * _carrier(blockSize, _AM_MOD_CYCLES, np.cos)
    carrier = _carrier(blockSize, _AM_CARRIER_CYCLES, np.cos)
    x = np.tile(modulation * carrier / (1.0 + _AM_MOD_DEPTH), (numChannels, 1))

    return (_assert_no_silence(testId, _quantize(x)),) + _params(_AM_CHANNELS)


# ---------------------------------------------------------------------------
# Registry. gen_test_cases_list.py emits one csv row per entry; limiter.py
# dispatches on testId and calls build().
# ---------------------------------------------------------------------------

DESIGNED_CASES = {
    64: {
        "numChannels": 4,
        "blockSize": 128,
        "build": build_static_probe,
        "description": "static probe: held levels incl. silence and 0 dBFS, alpha = 0",
    },
    65: {
        "numChannels": 2,
        "blockSize": 2048,
        "build": build_amp_sweep,
        "description": "dB-linear amplitude sweep -60 -> +6 -> -60 dBFS, swapped alphas",
    },
    66: {
        "numChannels": 2,
        "blockSize": 2048,
        "build": build_burst,
        "description": "burst -60 -> 0 -> -60 dBFS, attack 0.99 / release 0.999",
    },
    67: {
        "numChannels": 2,
        "blockSize": 1024,
        "build": build_am_sine,
        "description": "AM sine (paper Eq. 24), hard knee",
    },
}


def build(testId, numChannels, blockSize):
    """Build (x, threshold_dB, kneeWidth_dB, attackCoeff, releaseCoeff) for a case."""
    case = DESIGNED_CASES[testId]
    return case["build"](testId, numChannels, blockSize)
