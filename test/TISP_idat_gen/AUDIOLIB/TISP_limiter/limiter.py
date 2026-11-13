# Copyright (C) 2026 Texas Instruments Incorporated
#
# SPDX-License-Identifier: LicenseRef-TI-TFL

"""
TISP_limiter class implementation
"""

import os
import numpy as np
import sys

script_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.append(os.path.join(script_dir, "../../"))
# Import from common module
from common import Kernel
from common import AUDIOLIB_utils

sys.path.append(script_dir)
import designed_cases

# Import AUDIOLIB_idat_gen as audiolib
# If MCU_PLUS_SDK_ROOT is set, use it; otherwise fall back to relative path (OOB SDK layout)
mcu_sdk_root = os.environ.get("MCU_PLUS_SDK_ROOT")
if mcu_sdk_root:
    sys.path.append(os.path.join(mcu_sdk_root, "source", "audiolib", "test"))
else:
    sys.path.append(os.path.join(script_dir, "../../../../../audiolib/test"))

import AUDIOLIB_idat_gen as audiolib


def limiter_process(
    x,
    threshold_dB,
    kneeWidth_dB,
    attackCoeff,
    releaseCoeff,
    gainStateIn,
    numChannels,
    blockSize,
    db20_converter,
    undb20_converter,
):
    """Run the limiter algorithm on one interleaved block (see TISP_limiter.exec).

    Shared by the TISP_limiter test-data generator and the Limiter reference model.
    Returns (y, gainStateOut).
    """

    # Input is presented in interleaved format
    x_transposed = x.T

    # Initialize intermediate buffers
    gain_dB = np.zeros((numChannels, blockSize))

    # The envelope starts from rest unless the caller carries it over from the
    # previous block
    if gainStateIn is None:
        smoothedGain_dB = np.zeros(numChannels)
    else:
        smoothedGain_dB = np.asarray(gainStateIn, dtype=float).copy()

    # Envelope after the last sample, per channel (make-up gain excluded)
    gainStateOut = np.zeros(numChannels)

    # Complementary values of attack/release coefficients for each channel
    attackCoeffComplement = 1 - attackCoeff
    releaseCoeffComplement = 1 - releaseCoeff

    # STAGE 1: Compute absolute values for all samples using NumPy's vectorized operation
    x_abs = np.abs(x_transposed)

    # STAGE 2: Convert all samples to dB using the AUDIOLIB_dB20 module
    # Update the dB20 converter parameters for the entire array
    db20_converter._inChannels = numChannels
    db20_converter._inSamples = blockSize
    x_dB = db20_converter.exec(x_abs)

    # STAGE 3: Apply limiter algorithm to all samples
    for ch in range(numChannels):
        threshold_dB_ch = threshold_dB[ch]
        kneeWidth_dB_ch = kneeWidth_dB[ch]
        attackCoeff_ch = attackCoeff[ch]
        releaseCoeff_ch = releaseCoeff[ch]
        attackCoeffComplement_ch = attackCoeffComplement[ch]
        releaseCoeffComplement_ch = releaseCoeffComplement[ch]
        smoothedGain_dB_ch = smoothedGain_dB[ch]

        # Automatic make-up gain is a per-channel CONSTANT: M = -x_sc(0 dB), i.e. the
        # static characteristic evaluated at full scale (MATLAB `limiter`,
        # MakeUpGainMode='Auto' - see the link in the docstring above). It depends only
        # on the threshold and the knee width, never on the instantaneous sample level.
        halfBandKneeWidth_dB_ch = kneeWidth_dB_ch / 2

        # Hard knee
        if kneeWidth_dB_ch <= 0:
            if 0 >= threshold_dB_ch:
                x_sc_at_0dB = threshold_dB_ch
            else:
                x_sc_at_0dB = 0
        # Soft knee
        else:
            if 0 < threshold_dB_ch - halfBandKneeWidth_dB_ch:
                x_sc_at_0dB = 0
            elif 0 > threshold_dB_ch + halfBandKneeWidth_dB_ch:
                x_sc_at_0dB = threshold_dB_ch
            else:
                x_sc_at_0dB = 0 - (
                    (0 - threshold_dB_ch + halfBandKneeWidth_dB_ch) ** 2
                ) / (2 * kneeWidth_dB_ch)

        makeUpGain_dB_ch = -x_sc_at_0dB

        # Loop-invariant: these depend only on this channel's parameters. Hoisted out of
        # the sample loop, matching the kernel, which computes them once per channel block.
        kneeStart_dB = threshold_dB_ch - halfBandKneeWidth_dB_ch
        kneeEnd_dB = threshold_dB_ch + halfBandKneeWidth_dB_ch

        for i in range(blockSize):
            x_dB_sample = x_dB[ch, i]

            # Hard knee
            if kneeWidth_dB_ch <= 0:
                if x_dB_sample >= threshold_dB_ch:
                    x_sc = threshold_dB_ch
                else:
                    x_sc = x_dB_sample
            # Soft knee
            else:
                if x_dB_sample < kneeStart_dB:
                    x_sc = x_dB_sample
                elif x_dB_sample > kneeEnd_dB:
                    x_sc = threshold_dB_ch
                else:
                    x_sc = x_dB_sample - (
                        (x_dB_sample - threshold_dB_ch + halfBandKneeWidth_dB_ch) ** 2
                    ) / (2 * kneeWidth_dB_ch)

            # Computed gain
            targetGain_dB = x_sc - x_dB_sample

            # Apply gain smoothing
            # Attack mode
            if targetGain_dB <= smoothedGain_dB_ch:
                smoothedGain_dB_ch = (
                    attackCoeff_ch * smoothedGain_dB_ch
                    + attackCoeffComplement_ch * targetGain_dB
                )
            # Release mode
            else:
                smoothedGain_dB_ch = (
                    releaseCoeff_ch * smoothedGain_dB_ch
                    + releaseCoeffComplement_ch * targetGain_dB
                )

            # Apply the constant automatic make-up gain
            gain_makeUp = smoothedGain_dB_ch + makeUpGain_dB_ch

            # Store the gain value for this sample
            gain_dB[ch, i] = gain_makeUp

        # Record the envelope after the last sample (make-up gain excluded)
        gainStateOut[ch] = smoothedGain_dB_ch

    # STAGE 4: Convert gain from dB to linear using the AUDIOLIB_undB20 module
    # Update the undB20 converter parameters for the entire array
    undb20_converter._inChannels = numChannels
    undb20_converter._inSamples = blockSize
    gain_linear = undb20_converter.exec(gain_dB)

    # Apply the gain to the input signal using NumPy's vectorized operation
    y_transposed = x_transposed * gain_linear

    # Re-interleave data
    y = y_transposed.T

    return y, gainStateOut


class Limiter:
    """Reference model of the limiter kernel (feed-forward, log-domain peak limiter).

    The input frame is interleaved [num_samples, num_channels]. The gain envelope
    (make-up gain excluded) is kept in self.gainState from one exec() to the next.
    """

    def __init__(
        self,
        *,
        num_samples,
        num_channels,
        in_dtype,
        pThresholdDb,
        pKneeWidthDb,
        pAttackCoeffs,
        pReleaseCoeffs,
        bypassMode=0,
        **_,
    ):
        self.num_samples = int(num_samples)
        self.num_channels = int(num_channels)
        self.dtype = np.dtype(in_dtype)
        if self.dtype != np.float32:
            raise ValueError(f"Limiter supports float32 only, got {self.dtype}")
        self.threshold_dB = np.asarray(pThresholdDb, dtype=np.float64).reshape(-1)
        self.kneeWidth_dB = np.asarray(pKneeWidthDb, dtype=np.float64).reshape(-1)
        self.attackCoeff = np.asarray(pAttackCoeffs, dtype=np.float64).reshape(-1)
        self.releaseCoeff = np.asarray(pReleaseCoeffs, dtype=np.float64).reshape(-1)
        self.bypass = bool(bypassMode)
        self._db20_converter = audiolib.AUDIOLIB_dB20(
            dType="float", inChannels=self.num_channels, inSamples=self.num_samples
        )
        self._undb20_converter = audiolib.AUDIOLIB_undB20(
            dType="float", inChannels=self.num_channels, inSamples=self.num_samples
        )
        self.init()

    def init(self):
        """Reset the carried gain envelope (dB, make-up excluded) to rest."""
        self.gainState = None

    def reset(self):
        self.init()

    def exec(self, x):
        """Limit one interleaved frame [num_samples, num_channels]; the envelope stays in self."""
        if self.bypass:
            # bypass copies the input and leaves the envelope untouched
            return x
        y, self.gainState = limiter_process(
            x,
            self.threshold_dB,
            self.kneeWidth_dB,
            self.attackCoeff,
            self.releaseCoeff,
            self.gainState,
            self.num_channels,
            self.num_samples,
            self._db20_converter,
            self._undb20_converter,
        )
        return y.astype(self.dtype)


class TISP_limiter(Kernel):
    def __init__(self, libRootDir, blockSize=None, numChannels=None):

        libName = "tisp"
        kernelName = "limiter"

        super().__init__(libName, libRootDir, kernelName)

        # Store blockSize and numChannels as instance variables
        self._blockSize = blockSize
        self._numChannels = numChannels

        # Initialize the dB20 and undB20 converters with blockSize and numChannels
        self._db20_converter = audiolib.AUDIOLIB_dB20(
            dType="float", inChannels=numChannels, inSamples=blockSize
        )
        self._undb20_converter = audiolib.AUDIOLIB_undB20(
            dType="float", inChannels=numChannels, inSamples=blockSize
        )
        self.init()

    def init(self):
        """Reset the carried gain envelope (dB, make-up excluded) to rest.

        exec() takes gainStateIn explicitly; a stateful caller keeps the
        gainStateOut of each block in self.gainState and passes it to the next.
        """
        self.gainState = None

    """ Kernel Core Algorithm """

    def exec(
        self,
        x,
        threshold_dB,
        kneeWidth_dB,
        attackCoeff,
        releaseCoeff,
        gainStateIn=None,
    ):
        """
        Audio limiter function

        Implements the feed-forward, log-domain limiter of Fig. 7(c) in
        Giannoulis/Massberg/Reiss, "Digital Dynamic Range Compressor Design - A
        Tutorial and Analysis" (JAES 60(6), 2012), matching the MATLAB `limiter`
        System object:

            abs -> dB -> gain computer -> level detector -> +M -> linear -> multiply

        Static characteristic is paper Eq. (4) with 1/R = 0 (limiter), the log-domain
        detector placement is Eq. (23) / Fig. 7(c), and gain smoothing is the smooth
        branching peak detector of Eq. (16).

        The constant automatic make-up gain M = -x_sc(0 dB) is NOT from the paper - Eq. (1)
        only establishes that M is a constant. The rule comes from the MATLAB `limiter`
        System object with MakeUpGainMode = 'Auto':
        https://www.mathworks.com/help/audio/ref/limiter-system-object.html

        Note the paper's Section 5 recommends the smooth DECOUPLED detector (Eq. 17) over
        the branching one used here - its Fig. 9 shows branching has higher THD. Branching
        is a deliberate MATLAB-parity choice.

        Parameters:
        - x: input signal in linear domain (interleaved format)
        - threshold_dB: threshold in dB where limiting is applied (array of size numChannels)
        - kneeWidth_dB: kneewidth in dB specifying the transition region (array of size numChannels)
        - attackCoeff: attack coefficient in range [0, 1) (array of size numChannels)
        - releaseCoeff: release coefficient in range [0, 1) (array of size numChannels)
        - gainStateIn: optional per-channel starting envelope in dB (make-up gain
          excluded), i.e. the gainStateOut of the previous block. None (default)
          starts from rest (0 dB), as the first block does.

        Returns:
        - y: limited audio signal in linear domain (interleaved format)
        - gainStateOut: per-channel envelope after the last sample, excluding the make-up
          gain. This is the quantity the node's pGainState buffer holds once the signal
          has been processed, so it is directly comparable against the kernel's state.

        """
        """
        Audio limiter function

        Implements the feed-forward, log-domain limiter of Fig. 7(c) in
        Giannoulis/Massberg/Reiss, "Digital Dynamic Range Compressor Design - A
        Tutorial and Analysis" (JAES 60(6), 2012), matching the MATLAB `limiter`
        System object:

            abs -> dB -> gain computer -> level detector -> +M -> linear -> multiply

        Static characteristic is paper Eq. (4) with 1/R = 0 (limiter), the log-domain
        detector placement is Eq. (23) / Fig. 7(c), and gain smoothing is the smooth
        branching peak detector of Eq. (16).

        The constant automatic make-up gain M = -x_sc(0 dB) is NOT from the paper - Eq. (1)
        only establishes that M is a constant. The rule comes from the MATLAB `limiter`
        System object with MakeUpGainMode = 'Auto':
        https://www.mathworks.com/help/audio/ref/limiter-system-object.html

        Note the paper's Section 5 recommends the smooth DECOUPLED detector (Eq. 17) over
        the branching one used here - its Fig. 9 shows branching has higher THD. Branching
        is a deliberate MATLAB-parity choice.

        Parameters:
        - x: input signal in linear domain (interleaved format)
        - threshold_dB: threshold in dB where limiting is applied (array of size numChannels)
        - kneeWidth_dB: kneewidth in dB specifying the transition region (array of size numChannels)
        - attackCoeff: attack coefficient in range [0, 1) (array of size numChannels)
        - releaseCoeff: release coefficient in range [0, 1) (array of size numChannels)
        - gainStateIn: optional per-channel starting envelope in dB (make-up gain
          excluded), i.e. the gainStateOut of the previous block. None (default)
          starts from rest (0 dB), as the first block does.

        Returns:
        - y: limited audio signal in linear domain (interleaved format)
        - gainStateOut: per-channel envelope after the last sample, excluding the make-up
          gain. This is the quantity the node's pGainState buffer holds once the signal
          has been processed, so it is directly comparable against the kernel's state.

        """

        return limiter_process(
            x,
            threshold_dB,
            kneeWidth_dB,
            attackCoeff,
            releaseCoeff,
            gainStateIn,
            self._numChannels,
            self._blockSize,
            self._db20_converter,
            self._undb20_converter,
        )

    """ Test Case Processing Functions """

    def process_test_case(
        self, testCasesCsv, licenseFileName, output_dir=None, gen_idat_header=False
    ):
        """
        Process test cases and write the results to header files.

        Args:
            test_cases: List of dictionaries containing test case parameters
            output_dir: Directory to write the output files
            licenseFileName: Path to license file

        Returns:
            None
        """

        # if output directory not given, proceed to use default location for test data
        if output_dir == None:
            output_dir = self.libRootDir + "/test/" + self.kernelDirName

        outputStaticData = f"{output_dir}/test_data"

        # Create output directory if it doesn't exist
        os.makedirs(output_dir, exist_ok=True)

        # read test cases from given location into a dataframe
        testCasesDf = self.read_test_cases(testCasesCsv)

        # number of entries
        numTestCases = len(testCasesDf)

        # extract the column names only for test parameters struct definition
        testParams = testCasesDf.columns.to_list()

        # TODO: add header struct entries for buffers (order matters)
        bufferParams = [
            "staticIn",
            "staticOut",
            "staticGainOut",
            "threshold_dB",
            "kneeWidth_dB",
            "attackCoeff",
            "releaseCoeff",
        ]

        testParamHeaderKeys = testParams + bufferParams

        # TODO: define the data types for each test parameter entry in csv (in order)
        testParamsDataType = ["uint32_t"] * len(testParams)
        testParamsBufferType = ["void*"] * len(bufferParams)

        testParamHeaderValues = testParamsDataType + testParamsBufferType

        # Define test parameter metadata structure
        testParamsHeaderStruct = dict(zip(testParamHeaderKeys, testParamHeaderValues))

        libHeader = "audiolib"

        if gen_idat_header:

            # TODO: write header idat file
            self.write_idat_header(
                testParamsHeaderStruct, licenseFileName, output_dir, libHeader
            )

        staticBufferAttribute = '__attribute__((section(".staticData"))) static '

        # metadata for each test case
        testParamsSourceStruct = []

        # Process each test case
        for i in range(0, numTestCases):

            testCaseData = []

            # Extract parameters from csv (data frame)
            _testId = int(testCasesDf.iloc[i]["testId"])
            _testPattern = testCasesDf.iloc[i]["testPattern"]
            _outputDataLocation = testCasesDf.iloc[i]["outputDataLocation"]
            _numReps = int(testCasesDf.iloc[i]["numReps"])

            # TODO: extract kernel specific parameters from csv
            _numChannels = int(testCasesDf.iloc[i]["numChannels"])
            _blockSize = int(testCasesDf.iloc[i]["blockSize"])
            _dataType = testCasesDf.iloc[i]["dataType"]
            # Number of back-to-back blocks the case runs; the envelope carries over
            # between them. Older case lists have no such column: one block.
            _numBlocks = (
                int(testCasesDf.iloc[i]["numBlocks"])
                if "numBlocks" in testCasesDf.columns
                else 1
            )

            # csvData = [_testId, _testPattern, _outputDataLocation, _numReps, _numChannels, _blockSize, _dataType]

            # get entire row of data
            csvData = list(testCasesDf.iloc[i])
            testCaseData = testCaseData + csvData

            # Update the instance variables for blockSize and numChannels
            self._blockSize = _blockSize
            self._numChannels = _numChannels

            # Update the converter parameters
            self._db20_converter._inChannels = _numChannels
            self._db20_converter._inSamples = _blockSize
            self._undb20_converter._inChannels = _numChannels
            self._undb20_converter._inSamples = _blockSize

            if _testId in designed_cases.DESIGNED_CASES:

                if _numBlocks != 1:
                    raise ValueError(
                        f"designed case {_testId} must run a single block, got {_numBlocks}"
                    )

                # Deterministic algorithm case: signal and parameters are designed,
                # not drawn. See designed_cases.py for what each one pins down.
                x, threshold_dB, kneeWidth_dB, attackCoeff, releaseCoeff = (
                    designed_cases.build(_testId, _numChannels, _blockSize)
                )

            else:

                # Geometry case: random sine, but seeded PER TEST CASE from the testId.
                # Seeding is what makes the emitted vectors a reference at all - test_data
                # is gitignored and regenerated per developer per build, so an unseeded
                # generator leaves nothing to bisect against and makes coverage of the
                # rarer parameter draws (a knee width that rounds to 0, for instance)
                # a matter of luck. Keying the seed on testId rather than a global
                # sequence keeps each case independent of iteration order and of any
                # cases added before it.
                rng = np.random.default_rng(_testId)

                amplitude    = rng.uniform(1.0, 2.5, _numChannels)
                frequency    = rng.uniform(2, 20, _numChannels)
                phase_shift  = rng.uniform(0, 2 * np.pi, _numChannels)
                threshold_dB = np.round(rng.uniform(-20, -3, _numChannels), 0)
                kneeWidth_dB = np.round(rng.uniform(0, 15, _numChannels), 0)
                attackCoeff  = np.round(rng.uniform(0.1, 0.9, _numChannels), 1)
                releaseCoeff = np.round(rng.uniform(0.1, 0.9, _numChannels), 1)

                _, x = AUDIOLIB_utils.generate_sine_wave(
                    amplitude=amplitude,
                    num_points=_blockSize * _numBlocks,
                    num_channels=_numChannels,
                    frequency=frequency,
                    phase_shift=phase_shift,
                )

                x = np.round(x, 6)

            # Interleave the data
            x = x.T

            # Process the blocks back to back, carrying the envelope from one block to
            # the next exactly as the node's pGainState buffer does. gainStateOut is the
            # envelope the buffer must hold once the last block has been processed, which
            # is what makes it comparable to the kernel.
            yBlocks = []
            gainStateOut = None
            for blk in range(_numBlocks):
                yBlk, gainStateOut = self.exec(
                    x=x[blk * _blockSize : (blk + 1) * _blockSize, :],
                    threshold_dB=threshold_dB,
                    kneeWidth_dB=kneeWidth_dB,
                    attackCoeff=attackCoeff,
                    releaseCoeff=releaseCoeff,
                    gainStateIn=gainStateOut,
                )
                yBlocks.append(yBlk)
            y = np.vstack(yBlocks)

            # Bypass copies the input to the output, leaving the limiter gain unapplied. The
            # node's exec() takes the bypassCopy() path and never runs the gain computer, so the
            # envelope state is left untouched too - hence the zeroed gainStateOut.
            if int(testCasesDf.iloc[i]["bypassMode"]):
                y = x.copy()
                gainStateOut = np.zeros(_numChannels)

            # Define the test buffer parameters metadata structure (keep same order as struct definition)
            testCaseBufferStruct = [
                [
                    x.flatten(),
                    f"staticRefInCase{_testId}",
                    "float",
                    "{:.6f}",
                    staticBufferAttribute,
                ],
                [
                    y.flatten(),
                    f"staticRefOutCase{_testId}",
                    "float",
                    "{:.6f}",
                    staticBufferAttribute,
                ],
                [
                    gainStateOut,
                    f"staticRefGainCase{_testId}",
                    "float",
                    "{:.6f}",
                    staticBufferAttribute,
                ],
                [
                    threshold_dB,
                    f"staticRefThresholdCase{_testId}",
                    "float",
                    "{:.9g}",
                    staticBufferAttribute,
                ],
                [
                    kneeWidth_dB,
                    f"staticRefKneeWidthCase{_testId}",
                    "float",
                    "{:.9g}",
                    staticBufferAttribute,
                ],
                [
                    attackCoeff,
                    f"staticRefAttackCoeffCase{_testId}",
                    "float",
                    "{:.9g}",
                    staticBufferAttribute,
                ],
                [
                    releaseCoeff,
                    f"staticRefReleaseCoeffCase{_testId}",
                    "float",
                    "{:.9g}",
                    staticBufferAttribute,
                ],
            ]

            # Write the data to a static header file
            header_file_path = os.path.join(
                outputStaticData, f"staticRefCase{_testId}.h"
            )

            # extract the data arrays from metadata structure
            arrayData = [inner_list[0] for inner_list in testCaseBufferStruct]
            arrayNames = [inner_list[1] for inner_list in testCaseBufferStruct]
            dataTypes = [inner_list[2] for inner_list in testCaseBufferStruct]
            formatStrs = [inner_list[3] for inner_list in testCaseBufferStruct]
            sectionAttributes = [inner_list[4] for inner_list in testCaseBufferStruct]

            self.write_arrays_to_header(
                data_arrays=arrayData,
                array_names=arrayNames,
                data_types=dataTypes,
                output_path=header_file_path,
                section_attributes=sectionAttributes,
                format_strs=formatStrs,
                indent=True,
                indent_params=["-nut", "-i4", "-l100"],
            )

            # append to test case data struct
            testCaseData += arrayNames

            structEntry = dict(zip(testParamHeaderKeys, testCaseData))

            # add test case to struct containing all test cases
            testParamsSourceStruct.append(structEntry)

            print(f"Wrote data to static header file: {header_file_path}")

        # write source idat file
        self.write_idat_source(testParamsSourceStruct, licenseFileName, output_dir)
