"""
TISP_downwardExpander class implementation
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


class TISP_downwardExpander(Kernel):
    def __init__(self, libRootDir, blockSize=None, numChannels=None):

        libName = "tisp"
        kernelName = "downwardExpander"

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

    """ Kernel Core Algorithm """

    def exec(
        self,
        x,
        threshold_dB,
        ratio,
        kneeWidth_dB,
        attackCoeff,
        releaseCoeff,
        holdSamples,
    ):
        """
        Audio dynamic range expander function

        Implements the feed-forward, log-domain expander of the MATLAB `expander` System
        object (https://www.mathworks.com/help/audio/ref/expander-system-object.html), which
        follows Giannoulis/Massberg/Reiss, "Digital Dynamic Range Compressor Design - A
        Tutorial and Analysis" (JAES 60(6), 2012):

            abs -> dB -> gain computer -> level detector (with hold) -> linear -> multiply

        Static characteristic (T = threshold, R = ratio, W = knee width):
            x < T - W/2             : x_sc = T + (x - T) * R
            T - W/2 <= x <= T + W/2 : x_sc = x + (1 - R) * (x - T - W/2)^2 / (2W)
            x > T + W/2             : x_sc = x

        Gain smoothing is the branching peak detector (paper Eq. 16) with MATLAB's hold
        time: while the computed gain falls (attack) a counter runs, and the smoothed gain
        is held until it exceeds holdSamples. A release sample restarts the counter, a sample
        with target == gain leaves it unchanged - see the header of TISP_downwardExpander.hpp
        for the exact rule (measured on the MATLAB expander). There is no make-up gain.

        Parameters:
        - x: input signal in linear domain (interleaved format)
        - threshold_dB: threshold in dB below which expansion is applied (array of size numChannels)
        - ratio: expansion ratio >= 1 (array of size numChannels)
        - kneeWidth_dB: kneewidth in dB specifying the transition region (array of size numChannels)
        - attackCoeff: attack coefficient in range [0, 1) (array of size numChannels)
        - releaseCoeff: release coefficient in range [0, 1) (array of size numChannels)
        - holdSamples: hold time in samples before attack smoothing starts (array of size numChannels)

        Returns:
        - y: expanded audio signal in linear domain (interleaved format)
        - gainStateOut: per-channel envelope after the last sample. This is the quantity
          the node's pGainState buffer holds once the signal has been processed, so it is
          directly comparable against the kernel's state.
        - holdStateOut: per-channel hold counter after the last sample, the quantity the
          node's pHoldState buffer holds.

        """

        # Input is presented in interleaved format
        x_transposed = x.T

        # Initialize intermediate buffers
        gain_dB = np.zeros((self._numChannels, self._blockSize))

        # Envelope after the last sample, per channel
        gainStateOut = np.zeros(self._numChannels)

        # Hold counter after the last sample, per channel
        holdStateOut = np.zeros(self._numChannels)

        # Complementary values of attack/release coefficients for each channel
        attackCoeffComplement = 1 - attackCoeff
        releaseCoeffComplement = 1 - releaseCoeff

        # STAGE 1: Compute absolute values for all samples using NumPy's vectorized operation
        x_abs = np.abs(x_transposed)

        # STAGE 2: Convert all samples to dB using the AUDIOLIB_dB20 module
        # Update the dB20 converter parameters for the entire array
        self._db20_converter._inChannels = self._numChannels
        self._db20_converter._inSamples = self._blockSize
        x_dB = self._db20_converter.exec(x_abs)

        # MATLAB floors the level at 20*log10(eps) = -313.0712 dB (the kernel does the same)
        x_dB = np.maximum(x_dB, -313.0712)

        # STAGE 3: Apply expander algorithm to all samples
        for ch in range(self._numChannels):
            threshold_dB_ch = threshold_dB[ch]
            ratio_ch = ratio[ch]
            kneeWidth_dB_ch = kneeWidth_dB[ch]
            attackCoeff_ch = attackCoeff[ch]
            releaseCoeff_ch = releaseCoeff[ch]
            holdSamples_ch = holdSamples[ch]
            attackCoeffComplement_ch = attackCoeffComplement[ch]
            releaseCoeffComplement_ch = releaseCoeffComplement[ch]
            releaseFloor_ch = -1.17549435e-38 if releaseCoeff_ch > 0 else 0.0

            # The envelope and hold counter always start from rest: this model sees the whole signal
            smoothedGain_dB_ch = 0.0
            holdCount_ch = 0

            # Loop-invariant: these depend only on this channel's parameters. Hoisted out of
            # the sample loop, matching the kernel, which computes them once per channel block.
            halfBandKneeWidth_dB_ch = kneeWidth_dB_ch / 2
            kneeStart_dB = threshold_dB_ch - halfBandKneeWidth_dB_ch
            kneeEnd_dB = threshold_dB_ch + halfBandKneeWidth_dB_ch

            for i in range(self._blockSize):
                x_dB_sample = x_dB[ch, i]

                # Hard knee
                if kneeWidth_dB_ch <= 0:
                    if x_dB_sample < threshold_dB_ch:
                        x_sc = threshold_dB_ch + (x_dB_sample - threshold_dB_ch) * ratio_ch
                    else:
                        x_sc = x_dB_sample
                # Soft knee
                else:
                    if x_dB_sample < kneeStart_dB:
                        x_sc = threshold_dB_ch + (x_dB_sample - threshold_dB_ch) * ratio_ch
                    elif x_dB_sample > kneeEnd_dB:
                        x_sc = x_dB_sample
                    else:
                        x_sc = x_dB_sample + (1 - ratio_ch) * (
                            (x_dB_sample - kneeEnd_dB) ** 2
                        ) / (2 * kneeWidth_dB_ch)

                # Computed gain
                targetGain_dB = x_sc - x_dB_sample

                # Apply gain smoothing
                # Release mode
                if targetGain_dB > smoothedGain_dB_ch:
                    holdCount_ch = 0
                    # A release toward 0 dB stops at -FLT_MIN instead of reaching 0 (the C7x flushes
                    # subnormals), so the next 0 dB target resets the hold counter, as in MATLAB.
                    smoothedGain_dB_ch = releaseCoeff_ch * smoothedGain_dB_ch + (
                        releaseCoeffComplement_ch * targetGain_dB + releaseFloor_ch
                    )
                # Attack mode, target <= gain (gain is held until hold time has elapsed). The hold
                # counter only advances when target is strictly below gain: at target == gain it stays
                # as it is (measured MATLAB rule), and both attack expressions reduce to the gain itself.
                else:
                    if targetGain_dB < smoothedGain_dB_ch:
                        holdCount_ch += 1
                    if holdCount_ch > holdSamples_ch:
                        smoothedGain_dB_ch = (
                            attackCoeff_ch * smoothedGain_dB_ch
                            + attackCoeffComplement_ch * targetGain_dB
                        )

                # Store the gain value for this sample
                gain_dB[ch, i] = smoothedGain_dB_ch

            # Record the envelope after the last sample
            gainStateOut[ch] = smoothedGain_dB_ch
            holdStateOut[ch] = holdCount_ch

        # STAGE 4: Convert gain from dB to linear using the AUDIOLIB_undB20 module
        # Update the undB20 converter parameters for the entire array
        self._undb20_converter._inChannels = self._numChannels
        self._undb20_converter._inSamples = self._blockSize
        gain_linear = self._undb20_converter.exec(gain_dB)

        # Apply the gain to the input signal using NumPy's vectorized operation
        y_transposed = x_transposed * gain_linear

        # Re-interleave data
        y = y_transposed.T

        return y, gainStateOut, holdStateOut

    def write_wav_output(self, testId, wavName, fs, xFile, params, bypass=False):
        """
        Run the WHOLE input file of a WAV test case through the model and write the result.

        Only the first blockSize samples go into the C header; this output is the full-length
        signal for listening and analysis. Its first blockSize samples equal the header's
        reference apart from the header input's 6-decimal rounding, since the model is causal
        and starts from rest either way. Written as 32-bit float to
        wav/out/case<testId>_<name>_out.wav.

        Args:
            xFile: whole input file, (numChannels, samples)
            params: (threshold_dB, ratio, kneeWidth_dB, attackCoeff, releaseCoeff, holdSamples)
        """
        from scipy.io import wavfile

        numChannels, numSamples = xFile.shape

        if bypass:
            y = xFile.T
        else:
            # exec() sizes itself and the dB converters from these; restore them for the header path
            blockSize = self._blockSize
            self._blockSize = numSamples
            self._db20_converter._inSamples = numSamples
            self._undb20_converter._inSamples = numSamples
            try:
                y, _, _ = self.exec(xFile.T, *params)
            finally:
                self._blockSize = blockSize
                self._db20_converter._inSamples = blockSize
                self._undb20_converter._inSamples = blockSize

        outDir = os.path.join(designed_cases.WAV_DIR, "out")
        os.makedirs(outDir, exist_ok=True)
        stem = os.path.splitext(os.path.basename(wavName))[0]
        outPath = os.path.join(outDir, f"case{testId}_{stem}_out.wav")
        wavfile.write(outPath, fs, np.squeeze(y).astype(np.float32))
        print(f"  case {testId}: {wavName} ({numSamples} samples x {numChannels} ch) -> {outPath}")

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
        # The optional parameter columns are floats / per-channel lists, not uint32 test
        # parameters, so they stay out of the C struct (see designed_cases.PARAM_COLUMNS).
        testParams = [c for c in testCasesDf.columns.to_list() if c not in designed_cases.PARAM_COLUMNS]

        # TODO: add header struct entries for buffers (order matters)
        bufferParams = [
            "staticIn",
            "staticOut",
            "staticGainOut",
            "staticHoldOut",
            "threshold_dB",
            "ratio",
            "kneeWidth_dB",
            "attackCoeff",
            "releaseCoeff",
            "holdSamples",
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

            # The node runs numBlocks consecutive exec() calls of blockSize samples each, carrying its
            # state across them; the model processes all numSamples in one call from rest.
            _numBlocks = int(testCasesDf.iloc[i]["numBlocks"])
            _numSamples = _blockSize * _numBlocks

            # csvData = [_testId, _testPattern, _outputDataLocation, _numReps, _numChannels, _blockSize, _dataType]

            # get entire row of data
            csvData = [testCasesDf.iloc[i][c] for c in testParams]

            # Expander parameters given in the csv row, or None
            csvParams = designed_cases.csv_params(_testId, testCasesDf.iloc[i], int(testCasesDf.iloc[i]["numChannels"]))

            # A WAV file name in testPattern selects file input. The header field is a C
            # identifier, and the data is still compiled in, so emit STATIC there instead.
            isWavCase = designed_cases.is_wav_pattern(_testPattern)
            if isWavCase:
                csvData[testParams.index("testPattern")] = "STATIC"

            testCaseData = testCaseData + csvData

            # Update the instance variables for blockSize and numChannels
            self._blockSize = _numSamples
            self._numChannels = _numChannels

            # Update the converter parameters
            self._db20_converter._inChannels = _numChannels
            self._db20_converter._inSamples = _numSamples
            self._undb20_converter._inChannels = _numChannels
            self._undb20_converter._inSamples = _numSamples

            if isWavCase:

                # File case: the first numSamples samples of the WAV are the test vector.
                fs, xFile, params = designed_cases.build_wav_pattern(
                    _testId, str(_testPattern).strip(), _numChannels, _numSamples, csvParams
                )
                threshold_dB, ratio, kneeWidth_dB, attackCoeff, releaseCoeff, holdSamples = params
                x = np.round(xFile[:, :_numSamples], 6)

            elif _testId in designed_cases.DESIGNED_CASES:

                # Deterministic algorithm case: signal and parameters are designed,
                # not drawn. See designed_cases.py for what each one pins down.
                x, threshold_dB, ratio, kneeWidth_dB, attackCoeff, releaseCoeff, holdSamples = (
                    designed_cases.build(_testId, _numChannels, _numSamples)
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
                threshold_dB = np.round(rng.uniform(-30, -3, _numChannels), 0)
                ratio        = np.round(rng.uniform(1, 10, _numChannels), 1)
                kneeWidth_dB = np.round(rng.uniform(0, 15, _numChannels), 0)
                attackCoeff  = np.round(rng.uniform(0.1, 0.9, _numChannels), 1)
                releaseCoeff = np.round(rng.uniform(0.1, 0.9, _numChannels), 1)
                holdSamples  = np.round(rng.uniform(0, 8, _numChannels), 0)

                _, x = AUDIOLIB_utils.generate_sine_wave(
                    amplitude=amplitude,
                    num_points=_numSamples,
                    num_channels=_numChannels,
                    frequency=frequency,
                    phase_shift=phase_shift,
                )

                # Keep |x| >= 0.5 (-6 dBFS): the expansion gain then never falls below -27 dB at the
                # drawn ratios, inside the range the C7524 undB20 handles (it returns 0 below -37.9 dB).
                x = np.round(x + amplitude[:, np.newaxis] + 0.5, 6)

            # Parameters filled in the csv row override the designed / random ones
            if csvParams is not None and not isWavCase:
                threshold_dB, ratio, kneeWidth_dB, attackCoeff, releaseCoeff, holdSamples = csvParams

            # Interleave the data
            x = x.T

            # Process all numBlocks blocks through the expander model in one call.
            # gainStateOut / holdStateOut are what the node's pGainState / pHoldState buffers
            # must hold after its last exec(), which is what makes them comparable to the kernel.
            y, gainStateOut, holdStateOut = self.exec(
                x=x,
                threshold_dB=threshold_dB,
                ratio=ratio,
                kneeWidth_dB=kneeWidth_dB,
                attackCoeff=attackCoeff,
                releaseCoeff=releaseCoeff,
                holdSamples=holdSamples,
            )

            # Bypass copies the input to the output, leaving the expander gain unapplied. The
            # node's exec() takes the bypassCopy() path and never runs the gain computer, so the
            # envelope state is left untouched too - hence the zeroed gainStateOut / holdStateOut.
            if int(testCasesDf.iloc[i]["bypassMode"]):
                y = x.copy()
                gainStateOut = np.zeros(_numChannels)
                holdStateOut = np.zeros(_numChannels)

            if isWavCase:
                self.write_wav_output(
                    _testId,
                    str(_testPattern).strip(),
                    fs,
                    xFile,
                    (threshold_dB, ratio, kneeWidth_dB, attackCoeff, releaseCoeff, holdSamples),
                    bypass=int(testCasesDf.iloc[i]["bypassMode"]) != 0,
                )

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
                    "{:.9g}",
                    staticBufferAttribute,
                ],
                [
                    gainStateOut,
                    f"staticRefGainCase{_testId}",
                    "float",
                    "{:.9g}",
                    staticBufferAttribute,
                ],
                [
                    holdStateOut,
                    f"staticRefHoldCase{_testId}",
                    "float",
                    "{:.9g}",
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
                    ratio,
                    f"staticRefRatioCase{_testId}",
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
                [
                    holdSamples,
                    f"staticRefHoldSamplesCase{_testId}",
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
