"""
TISP_limiter class implementation
"""

import os
import numpy as np
import sys
import random

script_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.append(os.path.join(script_dir, "../../"))
# Import from common module
from common import Kernel
from common import AUDIOLIB_utils

# Import AUDIOLIB_idat_gen as audiolib
# If MCU_PLUS_SDK_ROOT is set, use it; otherwise fall back to relative path (OOB SDK layout)
mcu_sdk_root = os.environ.get("MCU_PLUS_SDK_ROOT")
if mcu_sdk_root:
    sys.path.append(os.path.join(mcu_sdk_root, "source", "audiolib", "test"))
else:
    sys.path.append(os.path.join(script_dir, "../../../../../audiolib/test"))

import AUDIOLIB_idat_gen as audiolib


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

    """ Kernel Core Algorithm """

    def exec(self, x, threshold_dB, kneeWidth_dB, attackCoeff, releaseCoeff):
        """
        Audio limiter function

        Parameters:
        - x: input signal in linear domain (interleaved format)
        - threshold_dB: threshold in dB where limiting is applied (array of size numChannels)
        - kneeWidth_dB: kneewidth in dB specifying the transition region (array of size numChannels)
        - attackCoeff: attack coefficient in range [0, 1) (array of size numChannels)
        - releaseCoeff: release coefficient in range [0, 1) (array of size numChannels)

        Returns:
        - y: limited audio signal in linear domain (interleaved format)
        - gain: gain values applied for input signal of each channel

        """

        # Input is presented in interleaved format
        x_transposed = x.T

        # Initialize output buffers
        y_transposed = np.zeros((self._numChannels, self._blockSize))
        gain = np.zeros(self._numChannels)

        # Initialize intermediate buffers
        gain_dB = np.zeros((self._numChannels, self._blockSize))

        # Initialize smoothed gain
        smoothedGain_dB = np.zeros(self._numChannels)

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

        # STAGE 3: Apply limiter algorithm to all samples
        for ch in range(self._numChannels):
            threshold_dB_ch = threshold_dB[ch]
            kneeWidth_dB_ch = kneeWidth_dB[ch]
            attackCoeff_ch = attackCoeff[ch]
            releaseCoeff_ch = releaseCoeff[ch]
            attackCoeffComplement_ch = attackCoeffComplement[ch]
            releaseCoeffComplement_ch = releaseCoeffComplement[ch]
            smoothedGain_dB_ch = smoothedGain_dB[ch]

            for i in range(self._blockSize):
                x_dB_sample = x_dB[ch, i]

                # Hard knee
                if kneeWidth_dB_ch <= 0:
                    if x_dB_sample >= threshold_dB_ch:
                        x_sc = threshold_dB_ch
                    else:
                        x_sc = x_dB_sample
                # Soft knee
                else:
                    halfBandKneeWidth_dB = kneeWidth_dB_ch / 2
                    kneeStart_dB = threshold_dB_ch - halfBandKneeWidth_dB
                    kneeEnd_dB = threshold_dB_ch + halfBandKneeWidth_dB

                    if x_dB_sample < kneeStart_dB:
                        x_sc = x_dB_sample
                    elif x_dB_sample > kneeEnd_dB:
                        x_sc = threshold_dB_ch
                    else:
                        x_sc = x_dB_sample - (
                            (x_dB_sample - threshold_dB_ch + halfBandKneeWidth_dB) ** 2
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

                # Apply automatic make-up gain
                if x_dB_sample == 0:
                    gain_makeUp = smoothedGain_dB_ch - x_sc
                else:
                    gain_makeUp = smoothedGain_dB_ch

                # Store the gain value for this sample
                gain_dB[ch, i] = gain_makeUp

            # Store the final smoothed gain for this channel
            gain[ch] = gain_makeUp

        # STAGE 4: Convert gain from dB to linear using the AUDIOLIB_undB20 module
        # Update the undB20 converter parameters for the entire array
        self._undb20_converter._inChannels = self._numChannels
        self._undb20_converter._inSamples = self._blockSize
        gain_linear = self._undb20_converter.exec(gain_dB)

        # Apply the gain to the input signal using NumPy's vectorized operation
        y_transposed = x_transposed * gain_linear

        # Re-interleave data
        y = y_transposed.T

        return y, gain

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

            # generate random sine input signal with given parameters
            amplitude = np.array(
                [random.uniform(1.0, 2.5) for _ in range(_numChannels)]
            )
            frequency = np.array([random.uniform(2, 20) for _ in range(_numChannels)])
            phase_shift = np.array(
                [random.uniform(0, 2 * np.pi) for _ in range(_numChannels)]
            )
            threshold_dB = np.round(
                np.array([np.random.uniform(-20, -3) for _ in range(_numChannels)]), 0
            )
            kneeWidth_dB = np.round(
                np.array([np.random.uniform(0, 15) for _ in range(_numChannels)]), 0
            )
            attackCoeff = np.round(
                np.array([np.random.uniform(0.1, 0.9) for _ in range(_numChannels)]), 1
            )
            releaseCoeff = np.round(
                np.array([np.random.uniform(0.1, 0.9) for _ in range(_numChannels)]), 1
            )
            _, x = AUDIOLIB_utils.generate_sine_wave(
                amplitude=amplitude,
                num_points=_blockSize,
                num_channels=_numChannels,
                frequency=frequency,
                phase_shift=phase_shift,
            )

            # TODO: check inputs for zeros
            # x = np.ones((_numChannels, _blockSize))
            x = np.round(x, 6)

            # Interleave the data
            x = x.T

            # Process the signal through the limiter model
            y, gain = self.exec(
                x=x,
                threshold_dB=threshold_dB,
                kneeWidth_dB=kneeWidth_dB,
                attackCoeff=attackCoeff,
                releaseCoeff=releaseCoeff,
            )

            # bypass copies the input to the output, leaving the limiter gain unapplied
            if int(testCasesDf.iloc[i]["bypassMode"]):
                y = x.copy()

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
                    gain,
                    f"staticRefGainCase{_testId}",
                    "float",
                    "{:.6f}",
                    staticBufferAttribute,
                ],
                [
                    threshold_dB,
                    f"staticRefThresholdCase{_testId}",
                    "float",
                    "{:.0f}",
                    staticBufferAttribute,
                ],
                [
                    kneeWidth_dB,
                    f"staticRefKneeWidthCase{_testId}",
                    "float",
                    "{:.0f}",
                    staticBufferAttribute,
                ],
                [
                    attackCoeff,
                    f"staticRefAttackCoeffCase{_testId}",
                    "float",
                    "{:.1f}",
                    staticBufferAttribute,
                ],
                [
                    releaseCoeff,
                    f"staticRefReleaseCoeffCase{_testId}",
                    "float",
                    "{:.1f}",
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
