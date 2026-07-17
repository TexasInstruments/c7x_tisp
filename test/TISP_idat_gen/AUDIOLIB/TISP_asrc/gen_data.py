from __future__ import print_function
from subprocess import call
from sys import platform

import sys
import numpy as np
import csv
import re

sys.path.append("../../common")
import TISP_utils
import file_io


def get_soc_guard(am275_soc_value):
    """Generate SOC preprocessor guard based on 'AM275 SOC' column value"""
    if am275_soc_value.lower() == "yes":
        return None
    else:
        return "!(SOC == AM275 && !defined(HOST_EMULATION))"


# Valid sample rates in Hz
VALID_SAMPLE_RATES = {32000, 44100, 48000}
# mapping (inputSampleRate, outputSampleRate) to filter coeff file names
FILTER_MAP = {
    # Upsampling (inputSampleRate < outputSampleRate)
    (32000, 32000): "h32_32kHz_to_48kHz.h",
    (32000, 44100): "h32_32kHz_to_48kHz.h",
    (32000, 48000): "h32_32kHz_to_48kHz.h",
    (44100, 44100): "h32_44_1kHz_to_48kHz.h",
    (44100, 48000): "h32_44_1kHz_to_48kHz.h",
    (48000, 48000): "h32_48kHz_to_48kHz.h",
    # Downsampling (inputSampleRate > outputSampleRate)
    (44100, 32000): "h32_44_1kHz_to_32kHz.h",
    (48000, 32000): "h32_48kHz_to_32kHz.h",
    (48000, 44100): "h32_48kHz_to_44_1kHz.h",
}


def lagrange(x, x0, y0):
    """
    LAGRANGE Interpolator
    y = lagrange(x, x0, y0) returns the interpolated value of y at x
    using the Lagrange interpolation formula.

    Input arguments:
    x: The point at which to interpolate
    x0: The x-coordinates of the data points
    y0: The y-coordinates of the data points
    """
    # Check if the input arguments are valid
    if len(x0) != len(y0):
        raise ValueError("x0 and y0 must have the same length")

    # Initialize the result
    y = 0

    # Loop over each data point
    for i in range(len(y0)):
        # Initialize the term
        term = y0[i]

        # Loop over each other data point
        for j in range(len(y0)):
            if i != j:
                # Update the term
                term *= (x - x0[j]) / (x0[i] - x0[j])

        # Update the result
        y += term

    return y


def get_sample_rate_code(sample_rate):
    sample_rate_table = {32000: 1, 44100: 2, 48000: 3}
    return sample_rate_table.get(
        sample_rate, 0
    )  # 0 is the default code for SAMPLE_RATE_NA


def gen_testCaseParams(testParamsFile, currPrm):
    """Generate and write test case parameters to idat file

    Args:
        param1 (file object): Input file object to idat.c file
        param2 (dict object): Dict object with test case parameter

    """

    testId = currPrm["ID"]
    dType = currPrm["sampleDataType"]
    sampleCount = currPrm["maxSampleCountPerBlock"]
    inputSampleRate = currPrm["inputSampleRate"]
    outputSampleRate = currPrm["outputSampleRate"]
    numChannels = currPrm["numChannels"]
    numBlocks = currPrm["numBlocks"]
    signalFrequency = currPrm["signalFrequency"]
    genOutSamplesPerChannel = currPrm["genOutSamplesPerChannel"]
    testType = currPrm["testType"]
    moduloFactor = currPrm["frameModuloFactor"]
    dataFormat = currPrm["dataFormat"]
    demoCase = currPrm["demoCase"]
    outputDataLocation = currPrm["outputDataLocation"]
    numReps = currPrm["numReps"]
    C7100 = currPrm["C7100"]
    AUDIOLIB_dType = TISP_utils.resolve_AUDIOLIB_dType(dType)
    am275_soc = currPrm.get("AM275 SOC", "yes")

    if testType == "STATIC":
        in0 = "staticRefInCase" + testId
        out = "staticRefOutCase" + testId
    else:
        in0 = "NULL"
        out = "NULL"

    if C7100 == "FALSE":
        testParamsFile.write("#if !defined(__C7100__)\n")

    if demoCase == "true":
        testParamsFile.write(
            "#if (defined(ALL_TEST_CASES) || (TEST_CASE == %d) || defined(DEMO_CASE))\n"
            % (int(testId))
        )
    else:
        testParamsFile.write(
            "#if (defined(ALL_TEST_CASES) || (TEST_CASE == %d))\n" % (int(testId))
        )

    soc_guard = get_soc_guard(am275_soc)
    if soc_guard:
        idatFile.write("#if %s\n" % soc_guard)

    idatFile.write("{\n")
    idatFile.write("%s, // Test Pattern \n" % (testType))
    idatFile.write("%s, // staticIn \n" % (in0))
    idatFile.write("%s, // staticOut \n" % (out))
    idatFile.write("%s, // Sample Data Type \n" % (AUDIOLIB_dType))
    idatFile.write("%d, // Max Sample Count Per Block \n" % (sampleCount))
    idatFile.write(
        "%d, // Input Sample Rate \n" % (get_sample_rate_code(inputSampleRate))
    )
    idatFile.write(
        "%d, // Output Sample Rate \n" % (get_sample_rate_code(outputSampleRate))
    )
    idatFile.write("%d, // Number of Channels \n" % (numChannels))
    idatFile.write("%d, // numBlocks \n" % (numBlocks))
    idatFile.write("%f, // Input Signal Frequency \n" % (signalFrequency))
    idatFile.write(
        "%d, // Generated output samples per channel \n" % (genOutSamplesPerChannel)
    )
    idatFile.write("%d, // Frame Modulo Factor \n" % (moduloFactor))
    idatFile.write("%s, // Data Format \n" % (dataFormat))
    idatFile.write("%s, // Output data location \n" % (outputDataLocation))
    idatFile.write("%d, // number of reps. \n" % (int(numReps)))
    idatFile.write("%d, // test ID\n" % int(testId))
    idatFile.write("},\n")
    if soc_guard:
        idatFile.write("#endif  // SOC guard\n")
    idatFile.write("#endif  // TEST_CASE guard\n")

    if C7100 == "FALSE":
        testParamsFile.write("#endif // if !defined(__C7100__)\n")

    idatFile.write("\n\n")


def gen_idat_file(idatFile, testCases):
    """Generate the idat file

    Args:
        param1 (file object): Input file object to idat.c file
        param2 (dict object): Dict object with test case parameter

    """

    fIn = open("../../common/ti_lice_header_idat.txt", "r")
    lines = fIn.readlines()
    fIn.close()
    idatFile.writelines(lines)
    includeString = '#include "TISP_asrc_idat.h"'
    idatFile.writelines(includeString)
    idatFile.write("\n")
    idatFile.write("/*This file is autogenerated*/\n")
    idatFile.write("/*Please do not edit*/\n")
    idatFile.write("\n\n")
    # generate ifdefs for header files
    for testCase in testCases:
        if testCase["testType"] == "STATIC":
            TISP_utils.gen_ifdefs(idatFile, testCase)

    # generate parameters for test cases
    idatFile.write("static TISP_asrc_testParams_t testParams[] =  ")
    idatFile.write("{\n")

    for testCase in testCases:
        gen_testCaseParams(idatFile, testCase)

    idatFile.write("};\n\n")
    fIn = open("test_param_function.txt", "r")
    lines = fIn.readlines()

    idatFile.writelines(lines)


def process_test_case(testCase):
    """Convert str to int for necessary parameters

    Args:
        Param1(:obj: dict) : Dictionary object with test case parameters

    Returns:
        Obj: Dictionary object with necessary parameters converted to int

    """

    # print(testCase.keys())
    testCase["maxSampleCountPerBlock"] = int(testCase["maxSampleCountPerBlock"])
    testCase["inputSampleRate"] = int(testCase["inputSampleRate"])
    testCase["outputSampleRate"] = int(testCase["outputSampleRate"])
    testCase["numChannels"] = int(testCase["numChannels"])
    testCase["numBlocks"] = int(testCase["numBlocks"])
    testCase["signalFrequency"] = float(testCase["signalFrequency"])
    testCase["genOutSamplesPerChannel"] = int(testCase["genOutSamplesPerChannel"])
    testCase["frameModuloFactor"] = int(testCase["frameModuloFactor"])

    # convert str to int for necessary parameters

    return testCase


def gen_test_case_header_file(testCase):
    """Generate test case and write out input, filter, and output tensor
    into a header file usable in C/C++ code.

    Args:
        param1 (:obj: dict): A dict object with all the test case
        parameters

    Returns:
        [int int]: Minimum and maximum value in output feature tensor

    """

    ##############################
    # Get test parameter of case #
    ##############################

    testId = testCase["ID"]
    dType = testCase["sampleDataType"]
    sampleCount = testCase["maxSampleCountPerBlock"]
    inputSampleRate = testCase["inputSampleRate"]
    outputSampleRate = testCase["outputSampleRate"]
    numChannels = testCase["numChannels"]
    numBlocks = testCase["numBlocks"]
    signalFrequency = testCase["signalFrequency"]
    moduloFactor = testCase["frameModuloFactor"]
    dataFormat = testCase["dataFormat"]

    NIn = sampleCount * numBlocks
    # Load filter coefficients
    U = 32  # upsampling ratio
    strFrequency = None
    # Get filter file name from dictionary, default to 'Unknown' if not found
    strFrequency = FILTER_MAP.get((inputSampleRate, outputSampleRate), "Unknown")

    filFile = f"filt_coeffs/{strFrequency}"

    # Read the file and extract the filter coefficients
    with open(filFile, "r") as f:
        lines = f.readlines()

    h = []
    for line in lines:
        # Check if the line contains copyright information or numeric values
        if re.search(r"//", line):
            continue
        values = re.findall(r"[-+]?[0-9]*\.?[0-9]+(?:[eE][-+]?[0-9]+)?", line)
        if values:
            h.extend([float(x) for x in values])

    # Pad h with zeros to match the length required by "arc2.c"
    NX = 3  # interpolator order (cubic)
    if (
        inputSampleRate <= outputSampleRate
    ):  # This only for Production code Upsampling Coeff
        h = np.concatenate((np.zeros(NX), np.array(h)))

    # Generate an array of i values
    i = np.arange(NIn)

    # Initialize the 2D array x with shape (numChannels, NIn)
    x = np.zeros((NIn))
    phase_offset = 1.045

    # Generate the input signal with phase offset for each channel
    out_list = []
    in_list = []
    for ch in range(numChannels):
        x = np.sin(
            2 * np.pi * i * signalFrequency / inputSampleRate + ch * phase_offset
        )
        in_list.append(x)

        # Zero-stuff input "x" by ratio 1:U -> "u"
        NU = NIn * U  # input length, after 1:U upsampling
        s0 = np.zeros((NIn, U))
        s0[:, 0] = x
        u = s0.flatten()

        # Convolve "h" with "u"
        q = np.convolve(h, u, mode="full")
        q = q[:NU]  # truncate to same length as "x"
        NQ = len(q)
        q = q * U  # compensate for zero stuffing & LPF

        # Perform sample rate conversion
        urho = U / (
            outputSampleRate / inputSampleRate
        )  # no. of (upsampled) inputs per output
        tbuf = np.zeros(NX + 1)  # interpolator input -- time values
        ibuf = np.zeros(NX + 1)  # interpolator input -- data values

        nx_beg = -int(np.floor(NX / 2))  # offset to 1st data value
        nx_end = nx_beg + NX  # offset to last data value
        utau = -nx_beg + urho  # min. value for valid indexing of q() below

        y = []
        while True:
            ntau = int(np.floor(utau))
            if 1 + ntau + nx_end > NQ:  # not enough input data?
                break
            tbuf = np.arange(ntau + nx_beg, ntau + nx_end + 1)
            ibuf = q[ntau + nx_beg : ntau + nx_end + 1]
            y.append(lagrange(utau, tbuf, ibuf))
            utau = utau + urho

        # store the generated sample count per channel
        testCase["genOutSamplesPerChannel"] = int(len(y))
        out_list.append(y)

    if dataFormat == "AUDIOLIB_DATA_FORMAT_INTERLEAVED":
        in_array = np.array(in_list)
        out_array = np.array(out_list)

        # Channel interleave the data
        in_interleaved = np.empty(in_array.size, dtype=in_array.dtype)
        out_interleaved = np.empty(out_array.size, dtype=out_array.dtype)

        # For input data
        for ch in range(numChannels):
            in_interleaved[ch::numChannels] = in_array[ch]

        # For output data
        for ch in range(numChannels):
            out_interleaved[ch::numChannels] = out_array[ch]

        headerFileName = "staticRefCase" + str(testId) + ".h"
        file_io.write_header_file(
            in_interleaved, out_interleaved, testCase, headerFileName
        )

        return [np.min(out_interleaved), np.max(out_interleaved)]
    else:
        in_list = np.array(in_list).flatten()
        out_list = np.array(out_list).flatten()

        headerFileName = "staticRefCase" + str(testId) + ".h"
        file_io.write_header_file(in_list, out_list, testCase, headerFileName)

        return [np.min(out_list), np.max(out_list)]


# CSV file with paratmeters for all test cases
testCasesCsvFile = "test_cases_list.csv"
testIdList = []  # list of test IDs that we want to generate test data
testIdList += range(0, 1000)
# testIdList = [18]

myArgs = TISP_utils.getCmdLineArgs()


def gen_test_case(testCase):
    if testCase["testType"] == "STATIC":
        [minY, maxY] = gen_test_case_header_file(testCase)
        print(
            "Test case generation completed for test ID",
            testCase["ID"],
            "w/ min:",
            minY,
            "and max",
            maxY,
        )
    else:
        print(
            "Skipping test case generation for test ID",
            testCase["ID"],
            "as tesType is RANDOM",
        )


#############################################################################
# Open CSV file with test cases and generate header file with test data for #
# test cases of interest; based on testIdList                               #
#############################################################################

with open(testCasesCsvFile, encoding="utf-8-sig") as csv_file:
    testCaseReader = csv.DictReader(csv_file)
    testCases = list(testCaseReader)

    # Add a new parameter to each test case
    for testCase in testCases:
        testCase["genOutSamplesPerChannel"] = 0

    for testCase in testCases:
        testCase = process_test_case(testCase)

    for testCase in testCases:
        if myArgs.allCases == True:
            gen_test_case(testCase)
        elif int(testCase["ID"]) in testIdList:
            gen_test_case(testCase)


idatFileName = "../../../AUDIOLIB/TISP_asrc/TISP_asrc_idat.c"


##########################################################################
# Generate idat.cpp file by parsing all the test cases in the CSV file # #
##########################################################################


with open(idatFileName, "w") as idatFile:
    gen_idat_file(idatFile, testCases)

# commented as indent takes a long time
# if platform == "linux" or platform == "linux2":
#     call(["clang-format", "-style=file", "-i", idatFileName])
#     call(["indent", "-nut", "-i3", "-c55", "-l200", idatFileName])
#     call(["rm", idatFileName + "~"])  # remove backup file


print("Idat file generation completed")
