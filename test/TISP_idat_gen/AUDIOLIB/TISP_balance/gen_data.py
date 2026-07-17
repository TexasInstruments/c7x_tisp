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
import os
import matplotlib.pyplot as plt


def generate_sine(
    num_samples, sampling_rate, num_frames, channel, frequency, amplitude
):
    """Generate a continuous sine wave for all numBlocks of a given channel.

    Args:
        num_samples (int): Number of samples per frame.
        sampling_rate (int): Sampling rate in Hz.
        num_frames (int): Number of numBlocks.
        channel (int): Channel index (for phase offset if needed).
        frequency (float, optional): Frequency of the sine wave in Hz. Defaults to 2000.0 Hz.
        amplitude (float, optional): Amplitude of the sine wave. Defaults to 1.0.

    Returns:
        numpy.ndarray: Array of sine wave samples, shape (num_samples * num_frames,).
    """
    total_samples = num_samples * num_frames
    t = np.linspace(0, total_samples / sampling_rate, total_samples, endpoint=False)
    sine_wave = amplitude * np.sin(2 * np.pi * frequency * t)
    return sine_wave


def gen_testCaseParams(testParamsFile, currPrm):
    """Generate and write test case parameters to idat file

    Args:
        param1 (file object): Input file object to idat.c file
        param2 (dict object): Dict object with test case parameter

    """

    testId = currPrm["ID"]
    dType = currPrm["dType"]
    numSamples = currPrm["numSamples"]
    numChannels = currPrm["numChannels"]
    balance = currPrm["balance"]
    samplingRate = currPrm["samplingRate"]
    smoothingTime = currPrm["smoothingTime"]
    strideInElements = currPrm["strideInElements"]
    strideOutElements = currPrm["strideOutElements"]
    testType = currPrm["testType"]
    demoCase = currPrm["demoCase"]
    outputDataLocation = currPrm["outputDataLocation"]
    numReps = currPrm["numReps"]
    C7100 = currPrm["C7100"]
    plotSignals = currPrm["plotSignals"]
    waveFiles = currPrm["waveFiles"]
    amplitude = currPrm["amplitude"]
    frequency = currPrm["frequency"]
    numBlocks = currPrm["numBlocks"]
    AUDIOLIB_dType = TISP_utils.resolve_AUDIOLIB_dType(dType)

    if testType in ["STATIC", "SINE"]:
        in_left = "staticRefInLeftCase" + testId
        out_left = "staticRefOutLeftCase" + testId
        in_right = "staticRefInRightCase" + testId
        out_right = "staticRefOutRightCase" + testId
        testType = "STATIC"
    else:
        in_left = "NULL"
        out_left = "NULL"
        in_right = "NULL"
        out_right = "NULL"

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

    idatFile.write("{\n")
    idatFile.write("%s, // Test Pattern \n" % (testType))
    idatFile.write("%s, // staticInLeft \n" % (in_left))
    idatFile.write("%s, // staticOutLeft \n" % (out_left))
    idatFile.write("%s, // staticInRight \n" % (in_right))
    idatFile.write("%s, // staticOutRight \n" % (out_right))
    idatFile.write("%s, // Sample Data Type \n" % (AUDIOLIB_dType))
    idatFile.write("%d, // Samples per channel \n" % (numSamples))
    idatFile.write("%d, // Number of Channels \n" % (numChannels))
    idatFile.write("%s, // Balance \n" % (balance))
    idatFile.write("%s, // Sampling Rate \n" % (samplingRate))
    idatFile.write("%s, // Smoothing Time \n" % (smoothingTime))
    # idatFile.write("%d, // Input Stride \n" % (strideInElements))
    idatFile.write(
        "AUDIOLIB_CALC_STRIDE(%s * sizeof(%s), AUDIOLIB_ALIGN_SHIFT_64BYTES), // strideInElements \n"
        % ((strideInElements), (AUDIOLIB_dType))
    )
    idatFile.write(
        "AUDIOLIB_CALC_STRIDE(%s * sizeof(%s), AUDIOLIB_ALIGN_SHIFT_64BYTES), // strideOutElements \n"
        % ((strideOutElements), (AUDIOLIB_dType))
    )
    # idatFile.write("%d, // Output Stride \n" % (strideOutElements))
    idatFile.write("%s, // Output data location \n" % (outputDataLocation))
    idatFile.write("%d, // number of reps. \n" % (int(numReps)))
    idatFile.write("%d, // test ID\n" % int(testId))
    idatFile.write("%d, //numBlocks\n" % int(numBlocks))
    idatFile.write("},\n")
    idatFile.write("#endif\n")

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
    includeString = '#include "TISP_balance_idat.h"'
    idatFile.writelines(includeString)
    idatFile.write("\n")
    idatFile.write("/*This file is autogenerated*/\n")
    idatFile.write("/*Please do not edit*/\n")
    idatFile.write("\n\n")
    # generate ifdefs for header files
    for testCase in testCases:
        if testCase["testType"] in ["STATIC", "SINE"]:
            TISP_utils.gen_ifdefs(idatFile, testCase)

    # generate parameters for test cases
    idatFile.write("static TISP_balance_testParams_t testParams[] =  ")
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
    testCase["testId"] = int(testCase["ID"])
    testCase["numSamples"] = int(testCase["numSamples"])
    testCase["numChannels"] = int(testCase["numChannels"])
    testCase["balance"] = float(testCase["balance"])
    testCase["samplingRate"] = int(testCase["samplingRate"])
    testCase["smoothingTime"] = float(testCase["smoothingTime"])
    testCase["strideInElements"] = int(testCase["strideInElements"])
    testCase["strideOutElements"] = int(testCase["strideOutElements"])
    testCase["numBlocks"] = int(
        testCase.get("numBlocks", 1)
    )  # Default to 1 if not specified
    testCase["plotSignals"] = testCase["plotSignals"].upper() == "TRUE"
    testCase["waveFiles"] = testCase["waveFiles"].upper() == "TRUE"
    testCase["amplitude"] = float(testCase.get("amplitude", 1.0))  # Default to 1.0
    testCase["frequency"] = float(
        testCase.get("frequency", 2000.0)
    )  # Default to 2000.0
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
    numSamples = testCase["numSamples"]
    numChannels = testCase["numChannels"]
    balance = testCase["balance"]
    samplingRate = testCase["samplingRate"]
    smoothingTime = testCase["smoothingTime"]
    plotSignals = testCase["plotSignals"]
    waveFiles = testCase["waveFiles"]
    testType = testCase["testType"]
    amplitude = testCase["amplitude"]
    frequency = testCase["frequency"]
    numBlocks = testCase["numBlocks"]
    samplesPerFrame = numSamples * numChannels  # Total samples per frame
    totalSamplesPerChannel = (
        numSamples * numBlocks
    )  # Total samples per channel across all numBlocks
    frameSize = numSamples  # Samples per channel per frame

    [minVal, maxVal] = [-10, 10]

    # compute target gains and smoothing coefficient
    angle = (1.0 + balance) * (np.pi / 4.0)
    targetGainL = np.cos(angle)
    targetGainR = np.sin(angle)
    # Set targetGainL to 0.0 if it is negligibly small
    if abs(targetGainL) < 1e-10:
        targetGainL = 0.0
    # Set targetGainR to 0.0 if it is negligibly small
    if abs(targetGainR) < 1e-10:
        targetGainR = 0.0

    pCurrentGainL = np.zeros(totalSamplesPerChannel, dtype=np.float32)
    pCurrentGainR = np.zeros(totalSamplesPerChannel, dtype=np.float32)

    # Compute gains frame by frame
    gainThreshold = 1e-6  # Threshold for considering gains equal to targets
    for frame in range(numBlocks):
        startIdx = frame * frameSize
        endIdx = (frame + 1) * frameSize
        # Check if smoothing should be bypassed for this frame
        bypassSmoothing = smoothingTime == 0.0
        if frame > 0:  # Check gain convergence for numBlocks after the first
            prev_idx = startIdx - 1
            bypassSmoothing |= (
                abs(pCurrentGainL[prev_idx] - targetGainL) < gainThreshold
                and abs(pCurrentGainR[prev_idx] - targetGainR) < gainThreshold
            )

        if bypassSmoothing:
            # No smoothing: Set all gains in this frame to target values
            pCurrentGainL[startIdx:endIdx] = targetGainL
            pCurrentGainR[startIdx:endIdx] = targetGainR
        else:
            # Compute smoothing coefficient
            smoothingTimeSec = smoothingTime / 1000.0  # Convert msec to seconds
            smoothingCoefficient = np.exp(-1.0 / (samplingRate * smoothingTimeSec))
            # Compute gains for the frame, starting from index 0
            for j in range(startIdx, endIdx):
                if frame == 0 and j == startIdx:
                    # Use initial gain of 0.0 for index 0 of frame 0
                    prevGainL = 0.0
                    prevGainR = 0.0
                else:
                    # Use previous frame's last gain or previous sample's gain
                    prevGainL = pCurrentGainL[j - 1]
                    prevGainR = pCurrentGainR[j - 1]
                pCurrentGainL[j] = (smoothingCoefficient * prevGainL) + (
                    (1.0 - smoothingCoefficient) * targetGainL
                )
                pCurrentGainR[j] = (smoothingCoefficient * prevGainR) + (
                    (1.0 - smoothingCoefficient) * targetGainR
                )

    # Generate random input samples for left and right channels based on test type frame by frame
    pInLocalLeft = np.zeros((numBlocks, samplesPerFrame), dtype=np.float32)
    pInLocalRight = np.zeros((numBlocks, samplesPerFrame), dtype=np.float32)
    for frame in range(numBlocks):
        if testCase["testType"] == "STATIC":
            # Generate random samples for each channel in the frame
            for i in range(numChannels):
                startIdx = i * numSamples
                endIdx = (i + 1) * numSamples
                pInLocalLeft[frame, startIdx:endIdx] = np.random.uniform(
                    low=minVal, high=maxVal, size=numSamples
                ).astype(np.float32)
                pInLocalRight[frame, startIdx:endIdx] = np.random.uniform(
                    low=minVal, high=maxVal, size=numSamples
                ).astype(np.float32)
        elif testCase["testType"] == "SINE":
            for i in range(numChannels):
                startIdx = i * numSamples
                endIdx = (i + 1) * numSamples
                # Generate continuous sine wave for all numBlocks of this channel
                sine_wave = generate_sine(
                    numSamples,
                    samplingRate,
                    numBlocks,
                    channel=i,
                    frequency=frequency,
                    amplitude=amplitude,
                )
                for frame in range(numBlocks):
                    frame_start = frame * numSamples
                    frame_end = (frame + 1) * numSamples
                    pInLocalLeft[frame, startIdx:endIdx] = sine_wave[
                        frame_start:frame_end
                    ]
                    pInLocalRight[frame, startIdx:endIdx] = sine_wave[
                        frame_start:frame_end
                    ]

    # Apply gains to input data frame by frame
    pOutLocalLeft = np.zeros((numBlocks, samplesPerFrame), dtype=np.float32)
    pOutLocalRight = np.zeros((numBlocks, samplesPerFrame), dtype=np.float32)
    for frame in range(numBlocks):
        gainStartIdx = frame * frameSize
        for i in range(numChannels):
            startIdx = i * numSamples
            endIdx = (i + 1) * numSamples
            for j in range(numSamples):
                gainIdx = gainStartIdx + j
                pOutLocalLeft[frame, startIdx + j] = (
                    pInLocalLeft[frame, startIdx + j] * pCurrentGainL[gainIdx]
                )
                pOutLocalRight[frame, startIdx + j] = (
                    pInLocalRight[frame, startIdx + j] * pCurrentGainR[gainIdx]
                )

    # Generate plots for channel 0 if plotSignals is TRUE
    if plotSignals:
        # Plot input signals (left and right) side by side
        file_io.plot_signal_pair(
            pInLocalLeft,
            pInLocalRight,
            numSamples,
            numChannels,
            numBlocks,
            testId,
            "input",
            channel=0,
            colors=["blue", "violet"],
            amplitude=amplitude,
            frequency=frequency,
            test_type=testType,
        )
        # Plot output signals (left and right) side by side
        file_io.plot_signal_pair(
            pOutLocalLeft,
            pOutLocalRight,
            numSamples,
            numChannels,
            numBlocks,
            testId,
            "output",
            channel=0,
            colors=["green", "red"],
            amplitude=amplitude,
            frequency=frequency,
            test_type=testType,
        )
        # Plot gain signals (left and right) side by side
        file_io.plot_signal_pair(
            pCurrentGainL,
            pCurrentGainR,
            numSamples,
            numChannels,
            numBlocks,
            testId,
            "gain",
            channel=None,
            colors=["brown", "gray"],
            amplitude=amplitude,
            frequency=frequency,
            test_type=testType,
        )

    # Generate WAV files if waveFiles is TRUE
    if waveFiles:
        file_io.write_wav_files(
            pInLocalLeft,
            pInLocalRight,
            pOutLocalLeft,
            pOutLocalRight,
            pCurrentGainL,
            pCurrentGainR,
            testCase,
            numBlocks,
        )

    # Prepare flattened 1D lists for left and right inputs, outputs, and gains
    in_left_list = pInLocalLeft.flatten().tolist()
    in_right_list = pInLocalRight.flatten().tolist()
    out_left_list = pOutLocalLeft.flatten().tolist()
    out_right_list = pOutLocalRight.flatten().tolist()
    gain_left_list = pCurrentGainL.flatten().tolist()  # New: Left gain values
    gain_right_list = pCurrentGainR.flatten().tolist()  # New: Right gain values

    # Write header file with flattened 1D inputs, outputs, and gains
    headerFileName = "staticRefCase" + str(testId) + ".h"
    file_io.write_header_file(
        in_left=in_left_list,
        in_right=in_right_list,
        out_left=out_left_list,
        out_right=out_right_list,
        gain_left=gain_left_list,
        gain_right=gain_right_list,
        testCase=testCase,
        headerFileName=headerFileName,
        frames=numBlocks,
    )

    # Return min and max across both left and right output channels
    min_output = min(np.min(out_left_list), np.min(out_right_list))
    max_output = max(np.max(out_left_list), np.max(out_right_list))
    return [min_output, max_output]


# CSV file with paratmeters for all test cases
testCasesCsvFile = "test_cases_list.csv"
testIdList = []  # list of test IDs that we want to generate test data
testIdList += range(0, 100)
# testIdList = [18]

myArgs = TISP_utils.getCmdLineArgs()


def gen_test_case(testCase):
    if testCase["testType"] in ["STATIC", "SINE"]:
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


idatFileName = "../../../AUDIOLIB/TISP_balance/TISP_balance_idat.c"


##########################################################################
# Generate idat.cpp file by parsing all the test cases in the CSV file # #
##########################################################################


with open(idatFileName, "w") as idatFile:
    gen_idat_file(idatFile, testCases)


if platform == "linux" or platform == "linux2":
    call(["clang-format", "-style=file", "-i", idatFileName])
    call(["indent", "-nut", "-i3", "-c55", "-l200", idatFileName])
    call(["rm", idatFileName + "~"])  # remove backup file


print("Idat file generation completed")
