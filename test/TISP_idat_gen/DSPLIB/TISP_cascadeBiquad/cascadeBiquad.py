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
        self, *, num_channels, num_samples, in_dtype, numStages, biquadForm, pCoeffs, bypassMode=0, precision="fp64", **_
    ):
        """
        Args:
            precision (str): Arithmetic precision for exec(), as in DSPLIB's
                DSPLIB_cascadeBiquad reference model.
                'fp64' (default) — scipy.signal.sosfilt in float64; maximum precision.
                'fp32' — form-specific float32 recursions that mirror the C kernel
                    operation-for-operation; use when the reference must match a
                    float32 C7x target.
        """
        self.numChannels = int(num_channels)
        self.numSamples = int(num_samples)
        self.dtype = np.dtype(in_dtype)
        self.numStages = int(numStages)
        self.biquadForm = biquadForm
        self.bypassMode = int(bypassMode)
        if precision not in ("fp64", "fp32"):
            raise ValueError(f"Unsupported precision: {precision!r}. Use 'fp64' or 'fp32'.")
        self.precision = precision

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

        # TI biquad form coefficients, grouped as DSPLIB's reference model groups them
        if self.biquadForm == "DSPLIB_BIQUAD_TIF":
            self.tifBiquadCascadeCoefs, self.tifBiquadCascadeGains = gen_tif_biquad_coefficients(
                self.sosCoefs, numStagesPerStageGroup=NUM_STAGES_PER_STAGE_GROUP
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
        """Zero initial state, shape (numStages, num_states, numChannels).

        fp32+DF1 requires 4 state variables per stage (xd1, xd2, yd1, yd2).
        All other precision/form combinations use 2 (sosfilt TDF2 layout for fp64;
        DF2/TDF2/TIF f32 loops also use 2 states).

        The state is held in the selected precision: float64 for fp64, float32 for
        fp32 (the kernel's filter variables are float32).
        """
        num_states = 4 if (self.precision == "fp32" and self.biquadForm == "DSPLIB_BIQUAD_DF1") else 2
        state_dtype = np.float32 if self.precision == "fp32" else np.float64
        self.state = np.zeros((self.sosCoefs.shape[0], num_states, self.numChannels), dtype=state_dtype)

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
    ):
        """SDK test signal: input and filter coefficients for a test case.

        Only gen_data.py calls this, before building the model (the coefficients are a
        constructor argument). Draws from np.random in a fixed order.

        Args:
            enableNChCoeffs: "False" for one coefficient set shared by all channels.

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
            frameOut: output of this frame, or a copy of the input when bypassMode
            is set. Bypass skips the filter and leaves self.state untouched, as
            TISP::DSPLIB::CascadeBiquad::exec() does (bypassCopy() only).

        Dispatches to the fp64 (default) or fp32 path based on self.precision.
        """
        if self.bypassMode:
            return frameIn.copy()
        if self.precision == "fp64":
            return self._exec_fp64(frameIn)
        return self._exec_fp32(frameIn)

    def _exec_fp64(self, frameIn):
        """fp64 path: scipy.signal.sosfilt for all forms (maximum precision)."""
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
        return frameOut

    def _exec_fp32(self, inputData):
        """fp32 path: form-specific float32 recursions matching the C kernel."""
        if self.biquadForm == "DSPLIB_BIQUAD_DF1":
            return self._exec_df1_f32(inputData)
        elif self.biquadForm == "DSPLIB_BIQUAD_DF2":
            return self._exec_df2_f32(inputData)
        elif self.biquadForm == "DSPLIB_BIQUAD_TDF2":
            return self._exec_tdf2_f32(inputData)
        elif self.biquadForm == "DSPLIB_BIQUAD_TIF":
            return self._exec_tif_f32(inputData)
        else:
            raise ValueError(f"Unsupported biquadForm: {self.biquadForm}")

    # ------------------------------------------------------------------
    # Form-specific float32 exec methods
    # ------------------------------------------------------------------

    def _exec_df1_f32(self, inputData):
        """
        Direct Form I (DF1) — replicates cascadeBiquad_cn.cpp:230.

        Recursion per stage per sample:
            output = b0*x + b1*xd1 + b2*xd2 - a1*yd1 - a2*yd2
            xd2 = xd1;  xd1 = x;  yd2 = yd1;  yd1 = output

        State layout: zi[stage, 0, ch] = xd1
                      zi[stage, 1, ch] = xd2
                      zi[stage, 2, ch] = yd1
                      zi[stage, 3, ch] = yd2

        SOS coefficient layout (standard scipy):
            sos[stage, :] = [b0, b1, b2, a0=1, a1, a2]
        """
        numSamples = inputData.shape[0]
        outputData = np.zeros((numSamples, self.numChannels), dtype=np.float32)

        # Work on a float32 copy of the input
        x_in = inputData.astype(np.float32)

        if not self.perChannel:
            # All channels share the same SOS coefficients (index 0)
            sos_f32 = self.sosCoefs[:, :, 0].astype(np.float32)  # (numStages, 6)
            b0 = sos_f32[:, 0]  # (numStages,)
            b1 = sos_f32[:, 1]
            b2 = sos_f32[:, 2]
            a1 = sos_f32[:, 4]
            a2 = sos_f32[:, 5]

            # State: (numStages, 4, numChannels) — all channels share coefs
            # zi[:, 0, :] = xd1, zi[:, 1, :] = xd2,
            # zi[:, 2, :] = yd1, zi[:, 3, :] = yd2
            zi = self.state.copy()

            for n in range(numSamples):
                x_stage = x_in[n, :].copy()  # (numChannels,)
                for s in range(self.numStages):
                    # .copy() makes reads independent of the writes below
                    xd1 = zi[s, 0, :].copy()   # (numChannels,)
                    xd2 = zi[s, 1, :].copy()
                    yd1 = zi[s, 2, :].copy()
                    yd2 = zi[s, 3, :].copy()

                    out = (
                        np.float32(b0[s]) * x_stage
                        + np.float32(b1[s]) * xd1
                        + np.float32(b2[s]) * xd2
                        - np.float32(a1[s]) * yd1
                        - np.float32(a2[s]) * yd2
                    )
                    out = out.astype(np.float32)

                    zi[s, 1, :] = xd1
                    zi[s, 0, :] = x_stage
                    zi[s, 3, :] = yd1
                    zi[s, 2, :] = out

                    x_stage = out

                outputData[n, :] = x_stage

            self.state = zi

        else:
            # Different coefficients per channel — loop channels separately
            for ch in range(self.numChannels):
                sos_f32 = self.sosCoefs[:, :, ch].astype(np.float32)
                b0 = sos_f32[:, 0]
                b1 = sos_f32[:, 1]
                b2 = sos_f32[:, 2]
                a1 = sos_f32[:, 4]
                a2 = sos_f32[:, 5]

                zi_ch = self.state[:, :, ch].copy()  # (numStages, 4)

                for n in range(numSamples):
                    x_stage = np.float32(x_in[n, ch])
                    for s in range(self.numStages):
                        xd1 = zi_ch[s, 0]
                        xd2 = zi_ch[s, 1]
                        yd1 = zi_ch[s, 2]
                        yd2 = zi_ch[s, 3]

                        out = np.float32(
                            np.float32(b0[s]) * x_stage
                            + np.float32(b1[s]) * xd1
                            + np.float32(b2[s]) * xd2
                            - np.float32(a1[s]) * yd1
                            - np.float32(a2[s]) * yd2
                        )

                        zi_ch[s, 1] = xd1
                        zi_ch[s, 0] = x_stage
                        zi_ch[s, 3] = yd1
                        zi_ch[s, 2] = out

                        x_stage = out

                    outputData[n, ch] = x_stage

                self.state[:, :, ch] = zi_ch

        return outputData

    def _exec_df2_f32(self, inputData):
        """
        Direct Form II (DF2) — replicates cascadeBiquad_cn.cpp:84.

        Recursion per stage per sample:
            wd0 = x - a1*wd1 - a2*wd2
            y   = b0*wd0 + b1*wd1 + b2*wd2
            wd2 = wd1;  wd1 = wd0

        State layout: zi[stage, 0, ch] = wd1
                      zi[stage, 1, ch] = wd2

        SOS coefficient layout (standard scipy):
            sos[stage, :] = [b0, b1, b2, a0=1, a1, a2]
        """
        numSamples = inputData.shape[0]
        outputData = np.zeros((numSamples, self.numChannels), dtype=np.float32)

        x_in = inputData.astype(np.float32)

        if not self.perChannel:
            sos_f32 = self.sosCoefs[:, :, 0].astype(np.float32)
            b0 = sos_f32[:, 0]
            b1 = sos_f32[:, 1]
            b2 = sos_f32[:, 2]
            a1 = sos_f32[:, 4]
            a2 = sos_f32[:, 5]

            zi = self.state.copy()  # (numStages, 2, numChannels)

            for n in range(numSamples):
                x_stage = x_in[n, :].copy()  # (numChannels,)
                for s in range(self.numStages):
                    wd1 = zi[s, 0, :]
                    wd2 = zi[s, 1, :]

                    wd0 = (
                        x_stage
                        - np.float32(a1[s]) * wd1
                        - np.float32(a2[s]) * wd2
                    ).astype(np.float32)

                    out = (
                        np.float32(b0[s]) * wd0
                        + np.float32(b1[s]) * wd1
                        + np.float32(b2[s]) * wd2
                    ).astype(np.float32)

                    zi[s, 1, :] = wd1
                    zi[s, 0, :] = wd0

                    x_stage = out

                outputData[n, :] = x_stage

            self.state = zi

        else:
            for ch in range(self.numChannels):
                sos_f32 = self.sosCoefs[:, :, ch].astype(np.float32)
                b0 = sos_f32[:, 0]
                b1 = sos_f32[:, 1]
                b2 = sos_f32[:, 2]
                a1 = sos_f32[:, 4]
                a2 = sos_f32[:, 5]

                zi_ch = self.state[:, :, ch].copy()

                for n in range(numSamples):
                    x_stage = np.float32(x_in[n, ch])
                    for s in range(self.numStages):
                        wd1 = zi_ch[s, 0]
                        wd2 = zi_ch[s, 1]

                        wd0 = np.float32(
                            x_stage
                            - np.float32(a1[s]) * wd1
                            - np.float32(a2[s]) * wd2
                        )
                        out = np.float32(
                            np.float32(b0[s]) * wd0
                            + np.float32(b1[s]) * wd1
                            + np.float32(b2[s]) * wd2
                        )

                        zi_ch[s, 1] = wd1
                        zi_ch[s, 0] = wd0

                        x_stage = out

                    outputData[n, ch] = x_stage

                self.state[:, :, ch] = zi_ch

        return outputData

    def _exec_tdf2_f32(self, inputData):
        """
        Transposed Direct Form II (TDF2) — replicates cascadeBiquad_cn.cpp:386.

        Recursion per stage per sample:
            y   = wd1 + x*b0
            wd1 = wd2 + x*b1 - a1*y
            wd2 = x*b2 - a2*y

        State layout: zi[stage, 0, ch] = wd1
                      zi[stage, 1, ch] = wd2

        SOS coefficient layout (standard scipy):
            sos[stage, :] = [b0, b1, b2, a0=1, a1, a2]
        """
        numSamples = inputData.shape[0]
        outputData = np.zeros((numSamples, self.numChannels), dtype=np.float32)

        x_in = inputData.astype(np.float32)

        if not self.perChannel:
            sos_f32 = self.sosCoefs[:, :, 0].astype(np.float32)
            b0 = sos_f32[:, 0]
            b1 = sos_f32[:, 1]
            b2 = sos_f32[:, 2]
            a1 = sos_f32[:, 4]
            a2 = sos_f32[:, 5]

            zi = self.state.copy()  # (numStages, 2, numChannels)

            for n in range(numSamples):
                x_stage = x_in[n, :].copy()  # (numChannels,)
                for s in range(self.numStages):
                    wd1 = zi[s, 0, :]
                    wd2 = zi[s, 1, :]

                    out = (wd1 + np.float32(b0[s]) * x_stage).astype(np.float32)
                    new_wd1 = (
                        wd2
                        + np.float32(b1[s]) * x_stage
                        - np.float32(a1[s]) * out
                    ).astype(np.float32)
                    new_wd2 = (
                        np.float32(b2[s]) * x_stage
                        - np.float32(a2[s]) * out
                    ).astype(np.float32)

                    zi[s, 0, :] = new_wd1
                    zi[s, 1, :] = new_wd2

                    x_stage = out

                outputData[n, :] = x_stage

            self.state = zi

        else:
            for ch in range(self.numChannels):
                sos_f32 = self.sosCoefs[:, :, ch].astype(np.float32)
                b0 = sos_f32[:, 0]
                b1 = sos_f32[:, 1]
                b2 = sos_f32[:, 2]
                a1 = sos_f32[:, 4]
                a2 = sos_f32[:, 5]

                zi_ch = self.state[:, :, ch].copy()

                for n in range(numSamples):
                    x_stage = np.float32(x_in[n, ch])
                    for s in range(self.numStages):
                        wd1 = zi_ch[s, 0]
                        wd2 = zi_ch[s, 1]

                        out = np.float32(wd1 + np.float32(b0[s]) * x_stage)
                        new_wd1 = np.float32(
                            wd2
                            + np.float32(b1[s]) * x_stage
                            - np.float32(a1[s]) * out
                        )
                        new_wd2 = np.float32(
                            np.float32(b2[s]) * x_stage
                            - np.float32(a2[s]) * out
                        )

                        zi_ch[s, 0] = new_wd1
                        zi_ch[s, 1] = new_wd2

                        x_stage = out

                    outputData[n, ch] = x_stage

                self.state[:, :, ch] = zi_ch

        return outputData

    def _exec_tif_f32(self, inputData):
        """
        Transposed Indirect Form (TIF) — replicates tif_lStage_ci.cpp.

        TIF uses pre-computed TI biquad coefficients stored in
        self.tifBiquadCascadeCoefs and aggregate per-group gains in
        self.tifBiquadCascadeGains.

        TI coefficient layout per stage (index in tifBiquadCascadeCoefs):
            [0] c0 = b1/b0 - a1
            [1] c1 = b2/b0 - a2
            [2] c2 = -a1  (used for new_d0 state update, maps to C kernel a11)
            [3] c3 = -a2  (used for new_d1 state update, maps to C kernel a12)

        Note: gen_tif_biquad_coefficients stores coef[2]=-a1 and coef[3]=-a2.
        The kernel stores them the other way round: DSPLIB_cascadeBiquad_utils.cpp
        (DSPLIB_cascadeBiquad_calc_tif_coefficients) writes
        [b1/b0 - a1, b2/b0 - a2, -a2, -a1], and the tif_*Stage_ci.cpp loops read
        a12 = coef[2], a11 = coef[3]. Using coef[2] for new_d0 and coef[3] for
        new_d1 here therefore applies the same a11 / a12 as the kernel.

        Recursion per stage per sample (using Python coefficient ordering):
            y      = x + d0
            d0_new = c2*d0 + c0*x + d1   (coef[2]=-a1 = C kernel a11)
            d1_new = c3*d0 + c1*x         (coef[3]=-a2 = C kernel a12)

        After the last stage in each stage group, apply the aggregate gain:
            y *= outGain   (product of b0 values for stages in that group)

        Stage grouping: groups of NUM_STAGES_PER_STAGE_GROUP (7) stages, as
        DSPLIB_cascadeBiquad_calc_tif_coefficients groups them (numStages <= 7 is
        a single group).

        State layout: zi[stage, 0, ch] = d0
                      zi[stage, 1, ch] = d1
        """
        numSamples = inputData.shape[0]
        outputData = np.zeros((numSamples, self.numChannels), dtype=np.float32)

        x_in = inputData.astype(np.float32)

        numStageGroups = math.ceil(self.numStages / NUM_STAGES_PER_STAGE_GROUP)

        if not self.perChannel:
            # tifBiquadCascadeCoefs shape: (numStages, 4, filterNumChannels=1)
            # tifBiquadCascadeGains shape: (numStageGroups, filterNumChannels=1)
            coefs_f32 = self.tifBiquadCascadeCoefs[:, :, 0].astype(np.float32)
            # shape: (numStages, 4)
            gains_f32 = self.tifBiquadCascadeGains[:, 0].astype(np.float32)
            # shape: (numStageGroups,)

            zi = self.state.copy()  # (numStages, 2, numChannels)

            for n in range(numSamples):
                x_stage = x_in[n, :].copy()  # (numChannels,)
                for grp in range(numStageGroups):
                    start_s = grp * NUM_STAGES_PER_STAGE_GROUP
                    end_s = min(
                        (grp + 1) * NUM_STAGES_PER_STAGE_GROUP, self.numStages
                    )
                    for s in range(start_s, end_s):
                        c0 = np.float32(coefs_f32[s, 0])
                        c1 = np.float32(coefs_f32[s, 1])
                        c2 = np.float32(coefs_f32[s, 2])
                        c3 = np.float32(coefs_f32[s, 3])

                        d0 = zi[s, 0, :]  # (numChannels,)
                        d1 = zi[s, 1, :]

                        out = (x_stage + d0).astype(np.float32)
                        # Match C kernel association: a11*d0 + (c0*x + d1)
                        new_d0 = (c2 * d0 + (c0 * x_stage + d1)).astype(np.float32)
                        new_d1 = (c3 * d0 + c1 * x_stage).astype(np.float32)

                        zi[s, 0, :] = new_d0
                        zi[s, 1, :] = new_d1

                        x_stage = out

                    # Apply aggregate gain at the end of each stage group
                    x_stage = (x_stage * np.float32(gains_f32[grp])).astype(np.float32)

                outputData[n, :] = x_stage

            self.state = zi

        else:
            # Different coefficients per channel
            for ch in range(self.numChannels):
                coefs_f32 = self.tifBiquadCascadeCoefs[:, :, ch].astype(np.float32)
                gains_f32 = self.tifBiquadCascadeGains[:, ch].astype(np.float32)

                zi_ch = self.state[:, :, ch].copy()  # (numStages, 2)

                for n in range(numSamples):
                    x_stage = np.float32(x_in[n, ch])
                    for grp in range(numStageGroups):
                        start_s = grp * NUM_STAGES_PER_STAGE_GROUP
                        end_s = min(
                            (grp + 1) * NUM_STAGES_PER_STAGE_GROUP,
                            self.numStages,
                        )
                        for s in range(start_s, end_s):
                            c0 = np.float32(coefs_f32[s, 0])
                            c1 = np.float32(coefs_f32[s, 1])
                            c2 = np.float32(coefs_f32[s, 2])
                            c3 = np.float32(coefs_f32[s, 3])

                            d0 = zi_ch[s, 0]
                            d1 = zi_ch[s, 1]

                            out = np.float32(x_stage + d0)
                            # Match C kernel association: a11*d0 + (c0*x + d1)
                            new_d0 = np.float32(c2 * d0 + np.float32(c0 * x_stage + d1))
                            new_d1 = np.float32(c3 * d0 + c1 * x_stage)

                            zi_ch[s, 0] = new_d0
                            zi_ch[s, 1] = new_d1

                            x_stage = out

                        x_stage = np.float32(x_stage * gains_f32[grp])

                    outputData[n, ch] = x_stage

                self.state[:, :, ch] = zi_ch

        return outputData
