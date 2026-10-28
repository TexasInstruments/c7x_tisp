"""
Deterministic, hand-designed algorithm test cases for TISP_downwardExpander.

The 60 cases in test_cases_list.csv are a GEOMETRY matrix: numChannels x blockSize,
stressing SE/SA predication, vector tails and buffer pitches with a randomly drawn
sine. They exercise every branch of the expander somewhere in the suite, but nothing
in them asserts a specific algorithmic behaviour.

The cases here are the complementary ALGORITHM test: no RNG, coefficients that are
physically meaningful, and signal geometry chosen so that one wrong term produces a
visibly wrong vector rather than a small numerical drift.

    A (64) static probe    - isolates the static characteristic, incl. both knee edges
    B (65) hold probe      - the hold counter: exact unity gain for holdSamples, then attack
    C (66) burst           - quiet -> loud -> quiet envelope transient
    D (67) amplitude sweep - attack/release asymmetry over the full dynamic range
    E (68) monotonic ramp  - static curve traced continuously by a decreasing DC ramp
    F (69) monotonic sine  - case E's ramp as the envelope of a sine carrier

Case ids continue the csv's sequence, after the geometry matrix and the bypass cases.
Because that makes them positional, gen_test_cases_list.py asserts the ids here are exactly
the next contiguous run - so adding a geometry or bypass case fails loudly instead of
silently colliding.

Notes that constrain these designs:

  * staticIn is emitted with "{:.6f}", which puts a quantization floor near -60 dBFS
    for a held level. Held levels therefore stay at or above -60 dBFS, and true
    silence is expressed as an exact 0.0 rather than approached with a very small number.

  * Exact-0.0 samples appear ONLY in case A, which runs with attackCoeff =
    releaseCoeff = 0. dB20 maps x <= 0 to the -758.6 dB floor, which an expander turns
    into thousands of dB of attenuation; with zero coefficients the envelope has no
    memory, so that stays local to the silent samples instead of taking the rest of the
    block to release.

  * Levels avoid sitting exactly on a knee breakpoint. staticIn is rounded to 6
    decimals, which nudges a level by a few 1e-6 dB, and the region tests are strict
    < / >, so a probe placed exactly on a breakpoint could land on either side.
    Every held level below is at least 0.5 dB clear of every channel's breakpoints.

  * The reference runs in float64, the kernel in float32. The one place the hold
    counter can tell them apart is an attack that converges EXACTLY onto a constant
    target (possible in float32, never in float64) followed by a further drop in level:
    float32 restarts the hold there, float64 does not. The held-level designs below
    therefore never step down twice in a row; every quiet segment follows a loud one.
"""

import os

import numpy as np

# Must match the format string used for staticIn / staticOut in downwardExpander.py.
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
    decimals. dB20 maps x <= 0 to its -758.6 dB floor, which the expander turns into
    thousands of dB of attenuation that a realistic release coefficient cannot recover
    from inside one block. Offsetting the phase by half a sample step puts every sample
    at least sin(pi/samplesPerCycle) away from a zero crossing.
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


def _held(levels_dBFS, hold, numChannels):
    """Concatenate held-level segments; the sign alternates so no segment is DC.

    The detector folds sign away with abs(), and a signed input proves it. None means
    true silence (exact 0.0).
    """
    segments = []
    for level in levels_dBFS:
        amplitude = 0.0 if level is None else _lin(level)
        segment = np.full(hold, amplitude)
        segment[1::2] *= -1.0
        segments.append(segment)
    return np.tile(np.concatenate(segments), (numChannels, 1))


def _params(channels):
    """Split a list of (threshold, ratio, knee, attack, release, hold) tuples into arrays."""
    arr = np.array(channels, dtype=np.float64)
    return tuple(arr[:, i].copy() for i in range(6))


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
# smoothedGain == targetGain, and holdSamples = 0 disables the hold, so every
# sample is an independent probe of the static characteristic with no envelope
# memory at all. That isolates the gain computer from the level detector.
#
# The invariants this case pins down:
#   * above the knee the output equals the input exactly (no make-up gain);
#   * ratio = 1 (channel 2) is an exact identity at every level;
#   * the soft knee is continuous at BOTH edges. Probes sit 1 dB inside and outside
#     each edge of channels 0 and 3, so a soft-knee term centred on the wrong edge
#     (x - T + W/2 instead of x - T - W/2) shows up as a dB-sized step.
# ---------------------------------------------------------------------------

_PROBE_LEVELS_DBFS = [
    None, -60.0, -50.0, -45.0, -40.0, -36.0, -33.0, -31.0,
    -28.0, -26.0, -24.0, -21.0, -15.0, -8.0, -3.0, 0.0,
]

_PROBE_HOLD = 8

_PROBE_CHANNELS = [
    # threshold, ratio, knee, attack, release, hold
    (-30.0, 4.0, 10.0, 0.0, 0.0, 0.0),  # soft knee (-35 .. -25)
    (-30.0, 4.0, 0.0, 0.0, 0.0, 0.0),   # hard knee
    (-20.0, 1.0, 6.0, 0.0, 0.0, 0.0),   # ratio 1: identity everywhere
    (-20.0, 2.0, 20.0, 0.0, 0.0, 0.0),  # wide soft knee (-30 .. -10)
]


def build_static_probe(testId, numChannels, blockSize):
    _check(testId, (len(_PROBE_CHANNELS), len(_PROBE_LEVELS_DBFS) * _PROBE_HOLD),
           (numChannels, blockSize))

    x = _held(_PROBE_LEVELS_DBFS, _PROBE_HOLD, numChannels)

    return (_quantize(x),) + _params(_PROBE_CHANNELS)


# ---------------------------------------------------------------------------
# B (65): hold probe, loud / quiet / loud / quiet / loud held levels
#
# Hard knee at -30 dB, ratio 4, so the quiet -50 dBFS level has a target gain of
# -60 dB and the loud -6 dBFS level a target of 0 dB. Each quiet segment follows a
# loud one, so the envelope enters it at 0 dB and the hold counter at zero.
#
# The invariant: within each quiet segment the output equals the input EXACTLY for
# the first holdSamples samples (gain still 0 dB), and only then starts to fall. The
# second quiet segment proves the release in between restarted the counter. The three
# channels differ only in holdSamples (0, 16, 48), so a hold that is ignored, off by
# one, or never restarted produces a visibly different vector per channel.
# ---------------------------------------------------------------------------

_HOLD_SEGMENTS = [
    # level (dBFS), length (samples)
    (-6.0, 64),
    (-50.0, 128),
    (-6.0, 64),
    (-50.0, 128),
    (-6.0, 128),
]

_HOLD_CHANNELS = [
    # threshold, ratio, knee, attack, release, hold
    (-30.0, 4.0, 0.0, 0.9, 0.5, 0.0),
    (-30.0, 4.0, 0.0, 0.9, 0.5, 16.0),
    (-30.0, 4.0, 0.0, 0.9, 0.5, 48.0),
]


def build_hold_probe(testId, numChannels, blockSize):
    _check(testId, (len(_HOLD_CHANNELS), sum(n for _, n in _HOLD_SEGMENTS)), (numChannels, blockSize))

    x = np.concatenate([_held([level], length, numChannels) for level, length in _HOLD_SEGMENTS], axis=1)

    return (_assert_no_silence(testId, _quantize(x)),) + _params(_HOLD_CHANNELS)


# ---------------------------------------------------------------------------
# C (66): burst, -50 dBFS -> 0 dBFS -> -50 dBFS on a sine carrier
#
# The noise-gate use case: a quiet bed that the expander pushes down, a loud burst
# it must open up for, and the tail it closes on again. Realistic asymmetric
# coefficients: attack (gain falling) 0.99, release (gain recovering) 0.95. The
# carrier's zero-crossing neighbourhoods inside the burst dip below the threshold,
# so the hold channel also shows hold re-triggering on every carrier half-cycle,
# while the hard-knee channel without hold follows them.
#
# No exact zeros here - see the module docstring.
# ---------------------------------------------------------------------------

_BURST_CHANNELS = [
    # threshold, ratio, knee, attack, release, hold
    (-30.0, 4.0, 10.0, 0.99, 0.95, 32.0),  # soft knee, with hold
    (-40.0, 8.0, 0.0, 0.99, 0.95, 0.0),    # hard knee, deeper threshold, no hold
]

_BURST_QUIET_DBFS = -35.0
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
# D (67): dB-linear amplitude sweep, -70 -> 0 -> -70 dBFS
#
# Linear in dB because the expander's interesting region is tens of dB below full
# scale, which a linear ramp would cross in a handful of samples. The two channels
# carry the same signal with attack/release SWAPPED. Equal or near-equal coefficients
# hide a swapped-branch bug; this pair cannot.
# ---------------------------------------------------------------------------

_SWEEP_CHANNELS = [
    # threshold, ratio, knee, attack, release, hold
    (-30.0, 3.0, 6.0, 0.95, 0.999, 8.0),  # fast attack, slow release
    (-30.0, 3.0, 6.0, 0.999, 0.95, 8.0),  # deliberately mirrored
]

_SWEEP_CARRIER_CYCLES = 64.0


def build_amp_sweep(testId, numChannels, blockSize):
    _check(testId, (len(_SWEEP_CHANNELS), 2048), (numChannels, blockSize))

    half = blockSize // 2
    envelope_dBFS = np.concatenate([
        np.linspace(-70.0, 0.0, half, endpoint=False),
        np.linspace(0.0, -70.0, blockSize - half, endpoint=False),
    ])

    carrier = _carrier(blockSize, _SWEEP_CARRIER_CYCLES)
    x = np.tile(_lin(envelope_dBFS) * carrier, (numChannels, 1))

    return (_assert_no_silence(testId, _quantize(x)),) + _params(_SWEEP_CHANNELS)


# ---------------------------------------------------------------------------
# E (68): monotonic ramp, DC falling 0 -> -60 dBFS, linear in dB
#
# Method 1 of the analysis plan: trace the whole static characteristic, not just the
# 16 points of case A. The signal is DC (no carrier, no sign change), so abs() is the
# level itself and x_dB is exactly the ramp - a sine would dip towards -inf dB at every
# zero crossing. attackCoeff = releaseCoeff = 0 and holdSamples = 0, as in case A, so
# each sample is an independent probe and output dB vs input dB is the static curve.
#
# The ramp stops at -60 dBFS, the quantization floor of the 6-decimal staticIn. At
# blockSize 1024 the step is ~0.059 dB per sample, which still leaves adjacent samples
# distinct after quantization; the build asserts the emitted ramp is strictly decreasing.
#
# Unlike case A the ramp passes arbitrarily close to the knee breakpoints, so a sample
# may be classified differently by the float64 reference and the float32 kernel. The
# characteristic is continuous at every breakpoint, so that costs only float-level error.
# ---------------------------------------------------------------------------

_RAMP_START_DBFS = 0.0
_RAMP_END_DBFS = -60.0


def build_monotonic_ramp(testId, numChannels, blockSize):
    _check(testId, (len(_PROBE_CHANNELS), 1024), (numChannels, blockSize))

    ramp = _quantize(_lin(np.linspace(_RAMP_START_DBFS, _RAMP_END_DBFS, blockSize)))
    if not np.all(np.diff(ramp) < 0):
        raise ValueError(f"designed case {testId}: ramp is not strictly decreasing after quantization")

    x = np.tile(ramp, (numChannels, 1))

    return (_assert_no_silence(testId, x),) + _params(_PROBE_CHANNELS)


# ---------------------------------------------------------------------------
# F (69): monotonic sine, envelope falling 0 -> -60 dBFS, linear in dB
#
# Case E's ramp put on a sine carrier: the envelope is strictly decreasing, the samples
# are not. Same channels and alpha = 0 as case E, so the two outputs compare directly:
# the detector is per-sample abs(), so instead of the clean static curve each carrier
# half-cycle is mapped through it, and the samples near the zero crossings sit far below
# the envelope level and are expanded hardest. That is the carrier effect the analysis
# has to account for whenever a tone is used to measure the expander.
#
# _carrier offsets the phase by half a sample step, so with 16 samples per cycle no
# sample is closer than sin(pi/16) ~ 0.195 of the envelope to zero - at -60 dBFS that is
# still ~2e-4, well clear of the 6-decimal quantization to exact 0.0.
# ---------------------------------------------------------------------------

_MONO_SINE_CARRIER_CYCLES = 64.0


def build_monotonic_sine(testId, numChannels, blockSize):
    _check(testId, (len(_PROBE_CHANNELS), 1024), (numChannels, blockSize))

    envelope = _lin(np.linspace(_RAMP_START_DBFS, _RAMP_END_DBFS, blockSize))
    carrier = _carrier(blockSize, _MONO_SINE_CARRIER_CYCLES)
    x = np.tile(envelope * carrier, (numChannels, 1))

    return (_assert_no_silence(testId, _quantize(x)),) + _params(_PROBE_CHANNELS)


# ---------------------------------------------------------------------------
# Registry. gen_test_cases_list.py emits one csv row per entry; downwardExpander.py
# dispatches on testId and calls build(). blockSize is the whole signal length; an optional
# numBlocks (default 1) splits it into that many consecutive exec() calls of the node.
# ---------------------------------------------------------------------------

DESIGNED_CASES = {
    64: {
        "numChannels": 4,
        "blockSize": 128,
        "build": build_static_probe,
        "description": "static probe: held levels incl. silence and both knee edges, alpha = 0",
    },
    65: {
        "numChannels": 3,
        "blockSize": 512,
        "numBlocks": 4,
        "build": build_hold_probe,
        "description": "hold probe: loud/quiet held levels, hold 0 / 16 / 48 samples",
    },
    66: {
        "numChannels": 2,
        "blockSize": 2048,
        "numBlocks": 4,
        "build": build_burst,
        "description": "burst -35 -> 0 -> -35 dBFS, attack 0.99 / release 0.95",
    },
    67: {
        "numChannels": 2,
        "blockSize": 2048,
        "build": build_amp_sweep,
        "description": "dB-linear amplitude sweep -70 -> 0 -> -70 dBFS, swapped alphas",
    },
    68: {
        "numChannels": 4,
        "blockSize": 1024,
        "build": build_monotonic_ramp,
        "description": "monotonic DC ramp 0 -> -60 dBFS, static curve, alpha = 0",
    },
    69: {
        "numChannels": 4,
        "blockSize": 1024,
        "build": build_monotonic_sine,
        "description": "monotonic sine, envelope 0 -> -60 dBFS, alpha = 0",
    },
}


WAV_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "wav")


def load_wav(path):
    """Read a WAV file as float64 (channels, samples); integer PCM is scaled to [-1, 1]."""
    from scipy.io import wavfile

    fs, data = wavfile.read(path)
    if np.issubdtype(data.dtype, np.integer):
        data = data.astype(np.float64) / float(np.iinfo(data.dtype).max)
    return fs, np.atleast_2d(np.asarray(data, dtype=np.float64).T)


# ---------------------------------------------------------------------------
# testPattern = <name>.wav
#
# A csv row whose testPattern is a WAV file name takes its input from that file instead
# of a synthesized signal. The name is resolved against wav/ unless it is absolute. The
# first blockSize samples become the test vector compiled into the header; downwardExpander.py
# also runs the WHOLE file through the model and writes wav/out/case<id>_<name>_out.wav
# for listening and analysis.
#
# Parameters come from the csv parameter columns (see PARAM_COLUMNS) when the row fills
# them, else WAV_DEFAULT_PARAMS on every channel.
# ---------------------------------------------------------------------------

WAV_DEFAULT_PARAMS = (-30.0, 4.0, 10.0, 0.99, 0.95, 32.0)

# Optional csv columns carrying the expander parameters, in node units: attackCoeff and
# releaseCoeff are the one-pole coefficients, holdSamples is in samples. A cell holds one
# value for every channel, or numChannels values separated by ';'. They are not uint32
# test parameters, so downwardExpander.py keeps them out of the C test-param struct; the values
# reach the kernel through the per-case staticRef*Case<id> arrays like any other case.
PARAM_COLUMNS = ["threshold_dB", "ratio", "kneeWidth_dB", "attackCoeff", "releaseCoeff", "holdSamples"]


def csv_params(testId, row, numChannels):
    """Parameter arrays from a csv row, or None when the row leaves every parameter column empty."""

    def cell(column):
        value = row[column] if column in row.index else None
        if value is None or (isinstance(value, float) and np.isnan(value)) or str(value).strip() == "":
            return None
        values = [float(v) for v in str(value).split(";")]
        if len(values) == 1:
            values = values * numChannels
        if len(values) != numChannels:
            raise ValueError(f"case {testId}: {column} has {len(values)} values, need 1 or {numChannels}")
        return np.array(values, dtype=np.float64)

    params = [cell(column) for column in PARAM_COLUMNS]
    if all(p is None for p in params):
        return None
    missing = [column for column, p in zip(PARAM_COLUMNS, params) if p is None]
    if missing:
        raise ValueError(f"case {testId}: parameter columns {missing} are empty; fill all six or none")
    return tuple(params)


def is_wav_pattern(testPattern):
    return str(testPattern).strip().lower().endswith(".wav")


def build_wav_pattern(testId, wavName, numChannels, blockSize, params=None):
    """Return (fs, xFile, params): the whole file as (numChannels, samples) and the param arrays.

    params is the csv_params() result; None falls back to WAV_DEFAULT_PARAMS on every channel.
    """
    path = wavName if os.path.isabs(wavName) else os.path.join(WAV_DIR, wavName)
    fs, data = load_wav(path)

    if data.shape[0] == 1:
        data = np.tile(data, (numChannels, 1))
    elif data.shape[0] != numChannels:
        raise ValueError(f"case {testId}: {wavName} has {data.shape[0]} channels, csv numChannels is {numChannels}")
    if data.shape[1] < blockSize:
        raise ValueError(f"case {testId}: {wavName} has {data.shape[1]} samples, csv blockSize is {blockSize}")

    if params is None:
        params = _params([WAV_DEFAULT_PARAMS] * numChannels)

    return fs, data, params


def build_from_wav(testId, numChannels, blockSize):
    """Input read from a WAV file instead of synthesized.

    The registry entry gives "wav" (path relative to this file), "params" (one
    (threshold, ratio, knee, attack, release, hold) tuple per channel) and optionally
    "start" (first sample, default 0). blockSize samples are taken from "start"; a mono
    file is copied to every channel, otherwise the file's channel count must match.
    Everything is compiled into the test header, so keep blockSize small.
    """
    case = DESIGNED_CASES[testId]
    _check(testId, (len(case["params"]), case["blockSize"]), (numChannels, blockSize))

    _, data = load_wav(os.path.join(os.path.dirname(os.path.abspath(__file__)), case["wav"]))

    start = case.get("start", 0)
    if start + blockSize > data.shape[1]:
        raise ValueError(
            f"designed case {testId}: {case['wav']} has {data.shape[1]} samples, "
            f"need {start + blockSize} (start {start} + blockSize {blockSize})"
        )
    if data.shape[0] == 1:
        data = np.tile(data, (numChannels, 1))
    elif data.shape[0] != numChannels:
        raise ValueError(f"designed case {testId}: {case['wav']} has {data.shape[0]} channels, need {numChannels}")

    x = data[:, start:start + blockSize]

    return (_quantize(x),) + _params(case["params"])


def build(testId, numChannels, blockSize):
    """Build (x, threshold_dB, ratio, kneeWidth_dB, attackCoeff, releaseCoeff, holdSamples) for a case."""
    case = DESIGNED_CASES[testId]
    return case["build"](testId, numChannels, blockSize)
