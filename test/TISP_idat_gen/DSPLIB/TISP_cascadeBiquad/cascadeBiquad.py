# Copyright (C) 2026 Texas Instruments Incorporated
#
# SPDX-License-Identifier: LicenseRef-TI-TFL

import math
import warnings

import numpy as np
from scipy import signal

# Number of coefficients per biquad, including a0 coefficieint
NUM_COEFS_PER_BIQUAD_FULL = 6
# Number of coefficients per biquad, excluding a0 coefficieint
NUM_COEFS_PER_BIQUAD = NUM_COEFS_PER_BIQUAD_FULL - 1
# Number of coefficients per TI biquad
NUM_COEFS_PER_TI_BIQUAD = NUM_COEFS_PER_BIQUAD - 1

# Number of states variables per DF2/TDF2 biquad
NUM_STATES_PER_DF2_BIQUAD = 2

# Number of stages per stage group when numStages exceeds MAX_SINGLE_GROUP_STAGES
NUM_STAGES_PER_STAGE_GROUP = 7
# Up to this many stages the kernel treats the whole cascade as ONE stage group
# (one aggregate gain); only above it are stages grouped by NUM_STAGES_PER_STAGE_GROUP.
# Mirrors DSPLIB_cascadeBiquad_tif_hStage_ci.cpp (numStages <= DSPLIB_CASCADEBIQUAD_NUM_STAGES_8).
MAX_SINGLE_GROUP_STAGES = 8


def stages_per_stage_group(numStages):
    """Stage-group size used by the TIF kernel for a cascade of numStages stages."""
    if numStages <= MAX_SINGLE_GROUP_STAGES:
        return numStages
    return NUM_STAGES_PER_STAGE_GROUP

# Cutoff frequencies normalized to Nyquist frequency
LPF_MIN_FREQ = 0.25  # Low pass filter cutoff minium frequency
LPF_RNG_FREQ = 0.25  # Low pass filter cutoff frequency range
HPF_MIN_FREQ = 0.5  # Low pass filter cutoff minium frequency
HPF_RNG_FREQ = 0.25  # Low pass filter cutoff frequency range
BF_LOW_MIN_FREQ = 0.25  # Bandpass/bandstop low cutoff minium frequency
BF_LOW_RNG_FREQ = 0.15  # Bandpass/bandstop low cutoff frequency range
BF_HIGH_MIN_FREQ = 0.6  # Bandpass/bandstop high cutoff minium frequency
BF_HIGH_RNG_FREQ = 0.15  # Bandpass/bandstop high cutoff frequency range

ENABLE_DEBUG_PRINT = 0  # Set to 1 to enable debug print


# Define function to catch a warning
def catch_warning(func, *args, **kwargs):
    with warnings.catch_warnings(record=True) as w:
        result = func(*args, **kwargs)
        if w:
            for warning in w:
                print("Warning caught:")
                print(warning.message)
                print(warning.category)
                print(warning.filename)
                print(warning.lineno)
    return result


def rescale_stage_gains(sosChan):
    """Spread the filter gain over all stages so that b0 != 1 in every stage.

    signal.tf2sos puts the whole filter gain into stage 1 and sets b0 = 1 in every
    later stage. Scaling each stage's numerator by g[s] (with the product of all g
    equal to 1) keeps the overall transfer function identical, but makes b0 != 1 in
    every stage, which is what exposes stage-group gain bugs in the TIF kernel.

    Args:
        sosChan (numpy.ndarray): (numStages, 6) SOS coefficients of one channel.

    Returns:
        numpy.ndarray: rescaled (numStages, 6) SOS coefficients.
    """
    numStages = sosChan.shape[0]
    # magnitudes kept away from 1 so b0 is clearly != 1; random sign is avoided to
    # keep the intermediate signal levels sensible
    g = np.where(
        np.random.rand(numStages) < 0.5,
        np.random.uniform(0.4, 0.8, numStages),
        np.random.uniform(1.25, 2.5, numStages),
    )
    g[0] = 1.0 / np.prod(g[1:])
    out = sosChan.copy()
    out[:, :3] *= g[:, None]
    return out


def gen_tif_biquad_coefficients(
    sosCoefs,
    numCoefsPerTiBiquad=NUM_COEFS_PER_TI_BIQUAD,
    numStagesPerStageGroup=None,
):
    """Generate TI biquad form coefficients

    Args:
        param1: 3D array of SOS coefficients.
                    Dim1: Number of SOS stages.
                    Dim2: Number of coefficients per stage (6).
                    Dim3: Number of channels.
        param2 (int): Number of coefficients per TI biquad.
        param3 (int): Number of stages per TI stage group (default: the kernel's
                    grouping, see stages_per_stage_group()).

    Returns:
        numpy.ndarray: 3D array of TI biquad coefficients
                    Dim1: Number of stages.
                    Dim2: Number of TI coefficients per stage (4).
                    Dim3: Number of channels.
        numpy.ndarray: 2D array of TI biquad gains
                    Dim1: Number of stage groups.
                    Dim2: Number of channels.

    """

    # Get parameters
    numStages = sosCoefs.shape[0]
    numChannels = sosCoefs.shape[2]

    ti_biquad_coefs = np.zeros((numStages, numCoefsPerTiBiquad, numChannels))
    for ch in range(numChannels):
        # Calculate biquad coefficients for each channel
        b0_r = 1 / sosCoefs[:, 0, ch]
        ti_biquad_coefs[:, [0, 1], ch] = b0_r[:, None] * sosCoefs[:, [1, 2], ch]
        ti_biquad_coefs[:, [2, 3], ch] = -sosCoefs[:, [4, 5], ch]
        ti_biquad_coefs[:, 0, ch] = (
            ti_biquad_coefs[:, 0, ch] + ti_biquad_coefs[:, 2, ch]
        )
        ti_biquad_coefs[:, 1, ch] = (
            ti_biquad_coefs[:, 1, ch] + ti_biquad_coefs[:, 3, ch]
        )

    # Compute aggregate gain, grouping stages the way the kernel does
    if numStagesPerStageGroup is None:
        numStagesPerStageGroup = stages_per_stage_group(numStages)
    numStageGroups = math.ceil(numStages / numStagesPerStageGroup)
    ti_gains = np.zeros((numStageGroups, numChannels))
    for ch in range(numChannels):
        for i in range(numStageGroups):
            start_idx = i * numStagesPerStageGroup
            end_idx = min((i + 1) * numStagesPerStageGroup, numStages)
            stage_group_sos_coefs = sosCoefs[start_idx:end_idx, :, ch]
            ti_gains[i, ch] = np.prod(stage_group_sos_coefs[:, 0])

    return ti_biquad_coefs, ti_gains


class CascadeBiquad:
    """Reference model of the cascadeBiquad kernel (SOS cascade, TDF2, stateful).

    One exec() call filters one frame [num_samples, num_channels]; the biquad state
    is kept in the object.
    """

    def __init__(
        self, *, num_channels, num_samples, in_dtype, numStages, biquadForm, pCoeffs, bypassMode=0, **_
    ):
        self.numChannels = int(num_channels)
        self.numSamples = int(num_samples)
        self.dtype = np.dtype(in_dtype)
        self.numStages = int(numStages)
        self.biquadForm = biquadForm
        self.bypassMode = int(bypassMode)

        # pCoeffs is the kernel's buffer: [numStages, 5] (b0 b1 b2 a1 a2, shared by all
        # channels) or [num_channels, numStages, 5] (per channel). Convert to the model's
        # SOS layout [numStages, 6, channels] (a0 = 1 inserted).
        pCoeffs = np.asarray(pCoeffs, dtype=np.float64)
        self.perChannel = pCoeffs.ndim == 3
        self.sosCoefs = np.insert(
            pCoeffs.transpose(1, 2, 0) if self.perChannel else pCoeffs[:, :, None],
            3,
            1.0,
            axis=1,
        )

        self.init()

    @staticmethod
    def sos_to_kernel_coeffs(sosCoefs, perChannel):
        """SOS coefficients [numStages, 6, channels] -> the kernel's pCoeffs layout
        ([numStages, 5], or [channels, numStages, 5] when perChannel)."""
        if perChannel:
            return np.delete(sosCoefs, 3, axis=1).transpose(2, 0, 1)
        return np.delete(sosCoefs[:, :, 0], 3, axis=1)

    def init(self):
        """Zero initial state, shape (numStages, 2, numChannels)."""
        self.state = np.zeros((self.sosCoefs.shape[0], 2, self.numChannels))

    def reset(self):
        """Return to the initial (zero) state."""
        self.init()

    @staticmethod
    def generate_inputs(
        dataType,
        dataSize,
        numChannels,
        numStages,
        enableNChCoeffs,
        biquadForm,
        numExecReps,
        nonUnitB0=False,
    ):
        """SDK test signal: input and filter coefficients for a test case.

        Only gen_data.py calls this, before building the model (the coefficients are a
        constructor argument). Draws from np.random in a fixed order.

        Args:
            enableNChCoeffs: "False" for one coefficient set shared by all channels.
            nonUnitB0: spread the gain over all stages so that b0 != 1 in every stage.

        Returns:
            (inputData, pCoeffs, sosCoefs, zi, tifBiquadCascadeCoefs, tifBiquadCascadeGains):
            inputData (numExecReps * dataSize, numChannels); pCoeffs in the kernel's
            layout (what the model takes); sosCoefs (numStages, 6, filterNumChannels) and
            zi, the initial state, as the header writer needs them.
        """

        if ENABLE_DEBUG_PRINT:
            # Print the test case parameters
            print("Test case parameters:")
            print("dataType: ", dataType)
            print("dataSize: ", dataSize)
            print("numChannels: ", numChannels)
            print("numStages: ", numStages)
            print("enableNChCoeffs: ", enableNChCoeffs)

        if dataType == "float":
            bits = 32
            bytes = bits // 8
        else:
            raise ValueError("Unsupported data type: %s" % dataType)

        #
        # Generate filter coefficients
        #

        # Set number of channels for coefficients generation
        if enableNChCoeffs == "False":
            # All channels use the same coefficients
            filterNumChannels = 1
        else:
            # Different coefficients for each channel
            filterNumChannels = numChannels
        # Initialize filter types
        filterTypes = ["low", "high", "bandpass", "bandstop"]  # butterworth filter types
        filterType = []
        # Initialize filter order and cutoff
        filterOrder = np.zeros(filterNumChannels)
        filterCutoff = np.zeros((filterNumChannels, 2))
        # Initialize biquad coefficients
        bCoefs = np.zeros((filterNumChannels, 2 * numStages + 1))
        aCoefs = np.zeros((filterNumChannels, 2 * numStages + 1))
        # Initialize SOS coefficients
        sosCoefs = np.zeros((numStages, NUM_COEFS_PER_BIQUAD_FULL, filterNumChannels))
        # Loop over channels for filter coefficient generation
        for ch in range(filterNumChannels):
            # Randomly select filter type
            filterType.append(np.random.choice(filterTypes))

            # Set filter order based on filter type
            # Randomly generate filter cutoffs based on filter type
            if filterType[ch] == "low":
                filterOrder[ch] = 2 * numStages
                filterCutoff[ch, 0] = LPF_MIN_FREQ + np.random.rand() * LPF_RNG_FREQ

                # design filter coefficients
                [b, a] = signal.butter(
                    filterOrder[ch], filterCutoff[ch, 0], btype=filterType[ch]
                )
                bCoefs[ch, : len(b)] = b
                aCoefs[ch, : len(a)] = a

            elif filterType[ch] == "high":
                filterOrder[ch] = 2 * numStages
                filterCutoff[ch, 0] = HPF_MIN_FREQ + np.random.rand() * HPF_RNG_FREQ

                # design filter coefficients
                [b, a] = signal.butter(
                    filterOrder[ch], filterCutoff[ch, 0], btype=filterType[ch]
                )
                bCoefs[ch, : len(b)] = b
                aCoefs[ch, : len(a)] = a

            elif filterType[ch] == "bandpass" or filterType[ch] == "bandstop":
                filterOrder[ch] = numStages
                filterCutoff[ch, 0] = BF_LOW_MIN_FREQ + np.random.rand() * BF_LOW_RNG_FREQ
                filterCutoff[ch, 1] = BF_HIGH_MIN_FREQ + np.random.rand() * BF_HIGH_RNG_FREQ

                # design filter coefficients
                [b, a] = signal.butter(
                    filterOrder[ch],
                    [filterCutoff[ch, 0], filterCutoff[ch, 1]],
                    btype=filterType[ch],
                )

                bCoefs[ch, : len(b)] = b
                aCoefs[ch, : len(a)] = a

            else:
                raise ValueError("Unsupported filter type: %s" % filterType[ch])

            # Design 2nd order sections
            sosChan = signal.tf2sos(bCoefs[ch, : len(b)], aCoefs[ch, : len(a)])
            # sosChan = catch_warning(signal.tf2sos, bCoefs[ch, :len(b)], aCoefs[ch, :len(a)])
            if nonUnitB0:
                sosChan = rescale_stage_gains(sosChan)
            sosCoefs[:, :, ch] = sosChan

        if ENABLE_DEBUG_PRINT:
            print("filterType: ", filterType)
            print("filterOrder: ", filterOrder)
            print("filterCutoff: ", filterCutoff)
            print("bCoefs: ", bCoefs)
            print("bCoefs shape: ", bCoefs.shape)
            print("aCoefs: ", aCoefs)
            print("aCoefs shape: ", aCoefs.shape)
            print("sosCoefs shape: ", sosCoefs.shape)
            print("sosCoefs: ", sosCoefs)

        # Generate input data
        inputData = np.random.uniform(
            low=-1.0, high=1.0, size=(numExecReps * dataSize, numChannels)
        ).astype("float")

        # Initial filter state (zero initial conditions)
        zi = np.zeros((numStages, 2, numChannels))
        pCoeffs = CascadeBiquad.sos_to_kernel_coeffs(sosCoefs, enableNChCoeffs != "False")

        if biquadForm == "DSPLIB_BIQUAD_TIF":
            # Generate TI biquad form coefficients
            tifBiquadCascadeCoefs, tifBiquadCascadeGains = gen_tif_biquad_coefficients(
                sosCoefs
            )

            if ENABLE_DEBUG_PRINT:
                print("TIF biquadCascadeCoefs shape: ", tifBiquadCascadeCoefs.shape)
                print("TIF biquadCascadeCoefs: ", tifBiquadCascadeCoefs)
                print("TIF biquadCascadeGains shape: ", tifBiquadCascadeGains.shape)
                print("TIF biquadCascadeGains: ", tifBiquadCascadeGains)

        else:
            tifBiquadCascadeCoefs = None
            tifBiquadCascadeGains = None

        return (
            inputData,
            pCoeffs,
            sosCoefs,
            zi,
            tifBiquadCascadeCoefs,
            tifBiquadCascadeGains,
        )

    def exec(self, frameIn):
        """Filter ONE frame using 2nd order sections, carrying filter state.

        Uses self.sosCoefs (numStages, 6, filterNumChannels) and self.state
        (numStages, 2, numChannels); self.state is updated to the state leaving
        this frame, so it holds the final state (zf) after the last frame.

        Args:
            frameIn: (num_samples, num_channels) input samples of this frame

        Returns:
            frameOut: output of this frame (a copy of the input when bypassMode is
            set; the state is updated regardless).

        """
        sosCoefs = self.sosCoefs
        zi = self.state
        numChannels = frameIn.shape[1]
        frameOut = np.zeros_like(frameIn)
        zf = np.zeros_like(zi)
        # Note sosfilt() uses TDF2 for filtering
        for ch in range(numChannels):
            coefCh = ch if self.perChannel else 0
            frameOut[:, ch], zf[:, :, ch] = signal.sosfilt(
                np.ascontiguousarray(sosCoefs[:, :, coefCh]),
                frameIn[:, ch],
                axis=0,
                zi=zi[:, :, ch],
            )
        self.state = zf
        if self.bypassMode:
            return frameIn.copy()
        return frameOut
