from __future__ import print_function
from subprocess import call
from sys import platform

import sys
import numpy as np
import csv
import re
import os

# Get the directory where the script is located
script_dir = os.path.dirname(os.path.abspath(__file__))
# Append the common directory relative to the script location
sys.path.append(os.path.join(script_dir, "../../common"))
import TISP_utils
import file_io


def get_soc_guard(am275_soc_value):
    """Generate SOC preprocessor guard based on 'AM275 SOC' column value"""
    if am275_soc_value.lower() == "yes":
        return None
    else:
        return "!(SOC == AM275 && !defined(HOST_EMULATION))"


def get_sample_rate_code(sample_rate):
    sample_rate_table = {
        8000: 0,
        11025: 1,
        12000: 2,
        16000: 3,
        22050: 4,
        24000: 5,
        32000: 6,
        44100: 7,
        48000: 8,
        64000: 9,
        88200: 10,
        96000: 11,
        128000: 12,
        176400: 13,
        192000: 14,
    }
    return sample_rate_table.get(
        sample_rate, 0
    )  # 0 is the default code for SAMPLE_RATE_NA


def convolve_full_debug(h, x):
    """
    Implements np.convolve(h, x, mode='full') using for loops for debugging purposes.

    Args:
        h: Filter coefficients array
        x: Input signal array

    Returns:
        y: Output array containing the full convolution result
    """
    h_len = len(h)
    x_len = len(x)
    y_len = h_len + x_len - 1

    # Initialize output array with zeros
    y = np.zeros(y_len)

    dbg_idex = 96

    # Perform convolution
    for n in range(y_len):
        acc = 0.0
        for k in range(h_len):
            x_idx = n - k
            if 0 <= x_idx < x_len:
                prod = h[k] * x[x_idx]
                acc += prod
                if n == dbg_idex:
                    print(
                        f"n={n}, k={k}, h[{k}]={h[k]:.9e}, x[{x_idx}]={x[x_idx]:.9e}, prod={prod:.9e}, acc={acc:.9e}"
                    )
        y[n] = acc
        if n == dbg_idex:
            print(f"y[{n}] = {y[n]:.9e}")

    return y


def gen_testCaseParams(testParamsFile, currPrm):
    """Generate and write test case parameters to idat file

    Args:
        param1 (file object): Input file object to idat.c file
        param2 (dict object): Dict object with test case parameter

    """

    testId = currPrm["ID"]
    dType = currPrm["sampleDataType"]
    sampleCount = currPrm["inputSampleCount"]
    inputSampleRate = currPrm["inputSampleRate"]
    outputSampleRate = currPrm["outputSampleRate"]
    numChannels = currPrm["numChannels"]
    numBlocks = currPrm["numBlocks"]
    signalFrequency = currPrm["signalFrequency"]
    enableMMA = currPrm["enableMMA"]
    testType = currPrm["testType"]
    dataFormat = currPrm["dataFormat"]
    testCategory = currPrm["testCategory"]
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
            "#if (defined(ALL_TEST_CASES) || (TEST_CASE == %d) || (TEST_CATEGORY == %d))\n"
            % (int(testId), int(testCategory))
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
    idatFile.write("%d, // Enable MMA \n" % (enableMMA))
    idatFile.write("%s, // Data Format \n" % (dataFormat))
    idatFile.write("%s, // Output data location \n" % (outputDataLocation))
    idatFile.write("%d, // number of reps. \n" % (int(numReps)))
    idatFile.write("%d, // test category \n" % (int(testCategory)))
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
    includeString = '#include "TISP_ssrc_idat.h"'
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
    idatFile.write("static TISP_ssrc_testParams_t testParams[] =  ")
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
    testCase["inputSampleCount"] = int(testCase["inputSampleCount"])
    testCase["inputSampleRate"] = int(testCase["inputSampleRate"])
    testCase["outputSampleRate"] = int(testCase["outputSampleRate"])
    testCase["numChannels"] = int(testCase["numChannels"])
    testCase["numBlocks"] = int(testCase["numBlocks"])
    testCase["signalFrequency"] = float(testCase["signalFrequency"])
    testCase["enableMMA"] = int(testCase["enableMMA"] == "TRUE")
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
    sampleCount = testCase["inputSampleCount"]
    inputSampleRate = testCase["inputSampleRate"]
    outputSampleRate = testCase["outputSampleRate"]
    numChannels = testCase["numChannels"]
    numBlocks = testCase["numBlocks"]
    signalFrequency = testCase["signalFrequency"]
    dataFormat = testCase["dataFormat"]

    NIn = sampleCount * numBlocks

    if outputSampleRate > 192000:
        print(f"Error: Unsupported output sample rate {outputSampleRate}.")
        sys.exit(1)

    if inputSampleRate < 8000:
        print(f"Error: Unsupported input sample rate {inputSampleRate}.")
        sys.exit(1)

    # Calculate the ratio between output and input sample rates
    if outputSampleRate > inputSampleRate:
        ratio = outputSampleRate / inputSampleRate
        is_upsampling = True
    else:
        ratio = inputSampleRate / outputSampleRate
        is_upsampling = False

    # Only use supported ratios (2 or 4)
    if ratio != 2 and ratio != 4:
        print(f"Error: Unsupported ratio {ratio}. Only ratios 2 and 4 are supported.")
        sys.exit(1)

    # Determine which filter files to use based on the ratio and whether upsampling or downsampling
    if is_upsampling:
        stage1_fil = "filt_coeffs/halfband_filter_stage1_upsample2x.h"
        stage2_fil = "filt_coeffs/halfband_filter_stage2_upsample2x.h"
    else:
        stage1_fil = "filt_coeffs/halfband_filter_stage1_downsample2x.h"
        stage2_fil = "filt_coeffs/halfband_filter_stage2_downsample2x.h"

    # Function to read filter coefficients from a file
    def read_filter_coeffs(file_path):
        coeffs = []
        try:
            with open(file_path, "r") as f:
                lines = f.readlines()

            for line in lines:
                # Check if the line contains copyright information or numeric values
                if re.search(r"//", line):
                    continue
                values = re.findall(r"[-+]?[0-9]*\.?[0-9]+(?:[eE][-+]?[0-9]+)?", line)
                if values:
                    coeffs.extend([float(x) for x in values])
            return coeffs
        except FileNotFoundError:
            print(f"Warning: Filter coefficient file {file_path} not found.")
            return []

    # Read filter coefficients
    if is_upsampling:
        h_stage1 = read_filter_coeffs(stage1_fil)
        if not h_stage1:
            print(f"Error: Could not read filter coefficients from {stage1_fil}")
            sys.exit(1)

        # If ratio is 4, also read stage2 filter coefficients
        if ratio == 4:
            h_stage2 = read_filter_coeffs(stage2_fil)
            if not h_stage2:
                print(f"Error: Could not read filter coefficients from {stage2_fil}")
                sys.exit(1)
    else:
        h_stage2 = read_filter_coeffs(stage2_fil)
        if not h_stage2:
            print(f"Error: Could not read filter coefficients from {stage2_fil}")
            sys.exit(1)

        # If ratio is 4, also read stage1 filter coefficients
        if ratio == 4:
            h_stage1 = read_filter_coeffs(stage1_fil)
            h_stage2 = read_filter_coeffs(stage2_fil)
            if not h_stage1:
                print(f"Error: Could not read filter coefficients from {stage1_fil}")
                sys.exit(1)

    # Generate an array of i values
    i = np.arange(NIn)

    # Initialize the input signal
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

        if is_upsampling:
            # Process the signal based on the ratio for upsampling
            if ratio == 2:
                # For ratio=2, use only stage1 filter
                # Zero-stuff input "x" by ratio 1:2 -> "u"
                NU = NIn * 2  # input length, after 1:2 upsampling
                s0 = np.zeros((NIn, 2))
                s0[:, 0] = x
                u = s0.flatten()

                # Convolve "h" with "u"
                q = np.convolve(h_stage1, u, mode="full")
                q = q[:NU]  # truncate to same length as "NU"
            else:  # ratio == 4
                # For ratio=4, apply stage1 filter first, then stage2 filter
                # First 2x upsampling
                NU1 = NIn * 2  # input length, after 1:2 upsampling
                s0 = np.zeros((NIn, 2))
                s0[:, 0] = x
                u1 = s0.flatten()

                # Convolve with stage1 filter
                q_stage1 = np.convolve(h_stage1, u1, mode="full")
                q_stage1 = q_stage1[:NU1]  # truncate to same length

                # Second 2x upsampling (total 4x)
                NU2 = NU1 * 2  # input length, after 2:4 upsampling
                s1 = np.zeros((NU1, 2))
                s1[:, 0] = q_stage1
                u2 = s1.flatten()

                # Convolve with stage2 filter
                q = np.convolve(h_stage2, u2, mode="full")
                q = q[:NU2]  # truncate to same length
        else:
            # Process the signal based on the ratio for downsampling
            if ratio == 2:
                # For ratio=2, use only stage1 filter for downsampling
                # Apply lowpass filter to input signal
                q_filtered = np.convolve(h_stage2, x, mode="full")
                # q_filtered = convolve_full_debug(h_stage2, x)
                q_filtered = q_filtered[:NIn]  # truncate to same length as input

                # Downsample by factor of 2 (take every other sample)
                q = q_filtered[::2]
            else:  # ratio == 4
                # For ratio=4, apply stage1 filter first, then stage2 filter
                # First apply stage1 filter
                q_stage1 = np.convolve(h_stage1, x, mode="full")
                q_stage1 = q_stage1[:NIn]  # truncate to same length as input

                # Downsample by factor of 2
                q_down1 = q_stage1[::2]

                # Apply stage2 filter
                q_stage2 = np.convolve(h_stage2, q_down1, mode="full")
                q_stage2 = q_stage2[: len(q_down1)]  # truncate to same length

                # Downsample by factor of 2 again (total 4x downsampling)
                q = q_stage2[::2]

        # Store the processed output for this channel
        out_list.append(q)

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
testIdList += range(0, 2000)
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

    for testCase in testCases:
        testCase = process_test_case(testCase)

    for testCase in testCases:
        if myArgs.allCases == True:
            gen_test_case(testCase)
        elif int(testCase["ID"]) in testIdList:
            gen_test_case(testCase)


idatFileName = "../../../AUDIOLIB/TISP_ssrc/TISP_ssrc_idat.c"


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
