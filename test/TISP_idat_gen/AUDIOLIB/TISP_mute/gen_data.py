from __future__ import print_function
from subprocess import call
from sys import platform

import sys
import numpy as np
import csv
import re
import pandas as pd

sys.path.append("../../common")
# from AUDIOLIB_idat_gen.common import TISP_utils
import TISP_utils
import file_io

import os
import matplotlib.pyplot as plt


def generate_sine(
    num_samples,
    sampling_rate,
    num_frames,
    channel,
    frequency,
    amplitude,
    num_channels=None,
    use_phase_offset=True,
):
    """Generate a continuous sine wave for all numBlocks of a given channel.

    Args:
        num_samples (int): Number of samples per frame.
        sampling_rate (int): Sampling rate in Hz.
        num_frames (int): Number of numBlocks.
        channel (int): Channel index (for phase offset if used).
        frequency (float): Frequency of the sine wave in Hz.
        amplitude (float): Amplitude of the sine wave.
        num_channels (int, optional): Number of channels (for phase offset).
        use_phase_offset (bool, optional): Whether to apply phase offset. Defaults to True.

    Returns:
        numpy.ndarray: Array of sine wave samples, shape (num_samples * num_frames,).
    """
    total_samples = num_samples * num_frames
    t = np.linspace(0, total_samples / sampling_rate, total_samples, endpoint=False)
    phase_offset = (
        (channel * np.pi / num_channels) if use_phase_offset and num_channels else 0.0
    )
    sine_wave = amplitude * np.sin(2 * np.pi * frequency * t + phase_offset)
    return sine_wave


def gen_testCaseParams(testParamsFile, currPrm):
    """Generate and write test case parameters to idat file.

    Args:
        testParamsFile (file object): Input file object to idat.c file.
        currPrm (dict): Dictionary object with test case parameters.
    """
    testId = currPrm["ID"]
    dType = currPrm["dType"]
    numSamples = currPrm["numSamples"]
    numChannels = currPrm["numChannels"]
    isMuted = currPrm["isMuted"]
    samplingRate = currPrm["samplingRate"]
    fadeTime = currPrm["fadeTime"]
    strideInElements = currPrm["strideInElements"]
    strideOutElements = currPrm["strideOutElements"]
    testType = currPrm["testType"]
    demoCase = currPrm["demoCase"]
    outputDataLocation = currPrm["outputDataLocation"]
    numReps = currPrm["numReps"]
    C7100 = currPrm["C7100"]
    numBlocks = currPrm["numBlocks"]
    plotSignals = currPrm["plotSignals"]
    waveFiles = currPrm["waveFiles"]
    amplitude = currPrm["amplitude"]
    frequency = currPrm["frequency"]
    isInterleaved = currPrm["isInterleaved"]
    fadeType = currPrm["fadeType"]
    AUDIOLIB_dType = TISP_utils.resolve_AUDIOLIB_dType(dType)

    # Map fadeType string to integer
    fadeType_map = {"LINEAR": 0, "SMOOTH": 1, "HARD": 2, "SYMMETRIC": 3}
    fadeType_int = fadeType_map[fadeType]

    if testType in ["STATIC", "SINE"]:
        in_data = "staticRefInCase" + testId
        out_data = "staticRefOutCase" + testId
        testType = "STATIC"
    else:
        in_data = "NULL"
        out_data = "NULL"

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
    idatFile.write("%s, // staticIn \n" % (in_data))
    idatFile.write("%s, // staticOut \n" % (out_data))
    idatFile.write("%s, // Sample Data Type \n" % (AUDIOLIB_dType))
    idatFile.write("%d, // Samples per channel \n" % (numSamples))
    idatFile.write("%d, // Number of Channels \n" % (numChannels))
    idatFile.write("%d, // isMuted \n" % (int(isMuted)))
    idatFile.write("%s, // Sampling Rate \n" % (samplingRate))
    idatFile.write("%s, // fadeTime \n" % (fadeTime))
    idatFile.write("%d, // isInterleaved \n" % (int(isInterleaved)))
    idatFile.write("%d, // fadeType (0=LINEAR, 1=SMOOTH, 2=HARD, 3=SYMMETRIC)\n" % (fadeType_int))
    idatFile.write(
        "AUDIOLIB_CALC_STRIDE(%s * sizeof(%s), AUDIOLIB_ALIGN_SHIFT_64BYTES), // strideInElements \n"
        % (strideInElements, AUDIOLIB_dType)
    )
    idatFile.write(
        "AUDIOLIB_CALC_STRIDE(%s * sizeof(%s), AUDIOLIB_ALIGN_SHIFT_64BYTES), // strideOutElements \n"
        % (strideOutElements, AUDIOLIB_dType)
    )
    idatFile.write("%s, // Output data location \n" % (outputDataLocation))
    idatFile.write("%d, // number of reps. \n" % (int(numReps)))
    idatFile.write("%d,// numBlocks \n" % (int(numBlocks)))
    idatFile.write("%d, // test ID\n" % (int(testId)))
    idatFile.write("},\n")
    idatFile.write("#endif\n")

    if C7100 == "FALSE":
        testParamsFile.write("#endif // if !defined(__C7100__)\n")

    idatFile.write("\n\n")


def gen_idat_file(idatFile, testCases):
    """Generate the idat file.

    Args:
        idatFile (file object): Input file object to idat.c file.
        testCases (list): List of test case dictionaries.
    """
    fIn = open("../../common/ti_lice_header_idat.txt", "r")
    lines = fIn.readlines()
    fIn.close()
    idatFile.writelines(lines)
    includeString = '#include "TISP_mute_idat.h"'
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
    idatFile.write("static TISP_mute_testParams_t testParams[] =  ")
    idatFile.write("{\n")

    for testCase in testCases:
        gen_testCaseParams(idatFile, testCase)

    idatFile.write("};\n\n")
    fIn = open("test_param_function.txt", "r")
    lines = fIn.readlines()
    idatFile.writelines(lines)


def process_test_case(testCase):
    """Convert str to int/float for necessary parameters.

    Args:
        testCase (dict): Dictionary object with test case parameters.

    Returns:
        dict: Dictionary with parameters converted to appropriate types.
    """
    testCase["testId"] = int(testCase["ID"])
    testCase["numSamples"] = int(testCase["numSamples"])
    testCase["numChannels"] = int(testCase["numChannels"])
    testCase["isMuted"] = float(testCase["isMuted"])
    testCase["samplingRate"] = int(testCase["samplingRate"])
    testCase["fadeTime"] = float(testCase["fadeTime"])
    testCase["strideInElements"] = int(testCase["strideInElements"])
    testCase["strideOutElements"] = int(testCase["strideOutElements"])
    testCase["numBlocks"] = int(testCase.get("numBlocks", 1))
    testCase["plotSignals"] = testCase["plotSignals"].upper() == "TRUE"
    testCase["waveFiles"] = testCase["waveFiles"].upper() == "TRUE"
    testCase["amplitude"] = float(testCase.get("amplitude", 1.0))
    testCase["frequency"] = float(testCase.get("frequency", 2000.0))
    testCase["isInterleaved"] = testCase["isInterleaved"].upper() == "TRUE"
    testCase["fadeType"] = testCase["fadeType"].upper()
    # Validate isInterleaved
    if testCase["isInterleaved"] not in [True, False]:
        raise ValueError(
            f"Invalid isInterleaved value {testCase['isInterleaved']} for test ID {testCase['ID']}"
        )
    # Validate fadeType
    if testCase["fadeType"] not in ["LINEAR", "SMOOTH", "HARD", "SYMMETRIC"]:
        raise ValueError(
            f"Invalid fadeType value {testCase['fadeType']} for test ID {testCase['ID']}"
        )
    # Set initialGain based on isMuted
    testCase["initialGain"] = 1.0 if testCase["isMuted"] == 1 else 0.0
    return testCase


def gen_test_case_header_file(testCase):
    """Generate test case and write out input, output, and gain tensors into a header file usable in C/C++ code.

    Args:
        testCase (dict): Dictionary with test case parameters.

    Returns:
        list: [min_output, max_output] across output channels.
    """
    testId = testCase["ID"]
    numSamples = testCase["numSamples"]
    numChannels = testCase["numChannels"]
    isMuted = testCase["isMuted"]
    samplingRate = testCase["samplingRate"]
    fadeTime = testCase["fadeTime"]
    numBlocks = testCase["numBlocks"]
    plotSignals = testCase["plotSignals"]
    waveFiles = testCase["waveFiles"]
    testType = testCase["testType"]
    amplitude = testCase["amplitude"]
    frequency = testCase["frequency"]
    isInterleaved = testCase["isInterleaved"]
    fadeType = testCase["fadeType"]
    current_gain = testCase["initialGain"]
    frameSize = numSamples
    samplesPerFrame = numSamples * numChannels
    totalSamplesPerChannel = numSamples * numBlocks

    [minVal, maxVal] = [-10, 10]

    # Compute target gain
    targetGain = 1.0 - isMuted
    pCurrentGain = np.zeros(totalSamplesPerChannel, dtype=np.float32)
    gainThreshold = 1e-6

    # Compute gains based on fadeType
    if fadeType == "HARD":
        # Instantaneous gain change to target
        pCurrentGain[:] = targetGain
    elif fadeType == "LINEAR":
        # Linear fade (from mute)
        timeConstantSec = fadeTime / 1000.0
        totalFadeSamples = timeConstantSec * samplingRate
        gainStep = (
            (targetGain - current_gain) / totalFadeSamples
            if totalFadeSamples > 0 and fadeTime > 0
            else 0
        )
        for i in range(totalSamplesPerChannel):
            if abs(current_gain - targetGain) < gainThreshold or fadeTime == 0:
                current_gain = targetGain
            else:
                current_gain += gainStep
                if (gainStep > 0 and current_gain > targetGain) or (
                    gainStep < 0 and current_gain < targetGain
                ):
                    current_gain = targetGain
            pCurrentGain[i] = current_gain
    elif fadeType == "SMOOTH":
        # Exponential smoothing
        for frame in range(numBlocks):
            startIdx = frame * frameSize
            endIdx = (frame + 1) * frameSize
            bypassSmoothing = fadeTime == 0.0
            if frame > 0:
                prev_idx = startIdx - 1
                bypassSmoothing |= (
                    abs(pCurrentGain[prev_idx] - targetGain) < gainThreshold
                )
            if bypassSmoothing:
                pCurrentGain[startIdx:endIdx] = targetGain
            else:
                timeConstantSec = fadeTime / 1000.0
                smoothingCoefficient = np.exp(-1.0 / (samplingRate * timeConstantSec))
                for j in range(startIdx, endIdx):
                    if frame == 0 and j == startIdx:
                        prevGain = current_gain
                    else:
                        prevGain = pCurrentGain[j - 1]
                    pCurrentGain[j] = (smoothingCoefficient * prevGain) + (
                        (1.0 - smoothingCoefficient) * targetGain
                    )
    elif fadeType == "SYMMETRIC":
        # Geometric ramp between the mute floor (-200 dB) and unity: equal time to mute and unmute
        floorGain = np.float32(1e-10)
        totalFadeSamples = (fadeTime / 1000.0) * samplingRate
        if totalFadeSamples <= 0:
            # No ramp: instantaneous change, exact zeros when muted
            pCurrentGain[:] = targetGain
        else:
            startGain = np.float32(1.0) if isMuted == 1.0 else floorGain
            stopGain = floorGain if isMuted == 1.0 else np.float32(1.0)
            gainStep = np.float32(
                np.power(np.float32(stopGain / startGain), np.float32(1.0 / totalFadeSamples))
            )
            rampGain = startGain
            for i in range(totalSamplesPerChannel):
                rampGain = np.float32(
                    min(max(np.float32(rampGain * gainStep), floorGain), np.float32(1.0))
                )
                # The floor is the fully muted state and outputs exact zeros
                pCurrentGain[i] = rampGain if rampGain > floorGain else 0.0

    # Generate input samples for all channels
    pInLocal = np.zeros((numBlocks, samplesPerFrame), dtype=np.float32)
    if testType == "STATIC":
        if isInterleaved:
            for frame in range(numBlocks):
                for j in range(numSamples):
                    for c in range(numChannels):
                        idx = j * numChannels + c
                        pInLocal[frame, idx] = np.random.uniform(
                            low=minVal, high=maxVal
                        )
        else:
            for frame in range(numBlocks):
                for i in range(numChannels):
                    startIdx = i * numSamples
                    endIdx = (i + 1) * numSamples
                    pInLocal[frame, startIdx:endIdx] = np.random.uniform(
                        low=minVal, high=maxVal, size=numSamples
                    ).astype(np.float32)
    elif testType == "SINE":
        if isInterleaved:
            for frame in range(numBlocks):
                for j in range(numSamples):
                    for c in range(numChannels):
                        idx = j * numChannels + c
                        frame_start = frame * numSamples
                        sine_wave = generate_sine(
                            numSamples,
                            samplingRate,
                            numBlocks,
                            c,
                            frequency,
                            amplitude,
                            numChannels,
                            use_phase_offset=True,
                        )
                        pInLocal[frame, idx] = sine_wave[frame_start + j]
        else:
            for i in range(numChannels):
                startIdx = i * numSamples
                endIdx = (i + 1) * numSamples
                sine_wave = generate_sine(
                    numSamples,
                    samplingRate,
                    numBlocks,
                    channel=i,
                    frequency=frequency,
                    amplitude=amplitude,
                    num_channels=numChannels,
                    use_phase_offset=True,
                )
                for frame in range(numBlocks):
                    frame_start = frame * numSamples
                    frame_end = (frame + 1) * numSamples
                    pInLocal[frame, startIdx:endIdx] = sine_wave[frame_start:frame_end]

    # Apply gains to input data
    pOutLocal = np.zeros((numBlocks, samplesPerFrame), dtype=np.float32)
    for frame in range(numBlocks):
        gainStartIdx = frame * frameSize
        if isInterleaved:
            for j in range(numSamples):
                for c in range(numChannels):
                    idx = j * numChannels + c
                    gainIdx = gainStartIdx + j
                    pOutLocal[frame, idx] = pInLocal[frame, idx] * pCurrentGain[gainIdx]
        else:
            for i in range(numChannels):
                startIdx = i * numSamples
                endIdx = (i + 1) * numSamples
                for j in range(numSamples):
                    gainIdx = gainStartIdx + j
                    pOutLocal[frame, startIdx + j] = (
                        pInLocal[frame, startIdx + j] * pCurrentGain[gainIdx]
                    )

    # Generate plots for channel 0 if plotSignals is TRUE
    if plotSignals:
        file_io.plot_signal_pair(
            pInLocal,
            pOutLocal,
            numSamples,
            numChannels,
            numBlocks,
            testId,
            "input_output",
            channel=0,
            colors=["blue", "green"],
            amplitude=amplitude,
            frequency=frequency,
            test_type=testType,
            is_interleaved=isInterleaved,
        )
        file_io.plot_signal_pair(
            pCurrentGain,
            None,
            numSamples,
            numChannels,
            numBlocks,
            testId,
            "gain",
            channel=None,
            colors=["brown"],
            amplitude=amplitude,
            frequency=frequency,
            test_type=testType,
            is_interleaved=isInterleaved,
        )

    # Generate WAV files if waveFiles is TRUE
    if waveFiles:
        file_io.write_wav_files(pInLocal, pOutLocal, pCurrentGain, testCase, numBlocks)

    # Prepare flattened 1D lists
    in_list = pInLocal.flatten().tolist()
    out_list = pOutLocal.flatten().tolist()
    gain_list = pCurrentGain.flatten().tolist()

    # Write header file
    headerFileName = "staticRefCase" + str(testId) + ".h"
    file_io.write_header_file(
        in_data=in_list,
        out_data=out_list,
        gain_data=gain_list,
        testCase=testCase,
        headerFileName=headerFileName,
        frames=numBlocks,
    )

    min_output = np.min(out_list)
    max_output = np.max(out_list)
    return [min_output, max_output]


testCasesCsvFile = "test_cases_list.csv"
testIdList = []  # list of test IDs that we want to generate test data
df = pd.read_csv(testCasesCsvFile)
rowLength = len(df)

# Convert string "[2,4]" → list [2, 4]
testIdList += range(0, rowLength + 1)
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
            "as testType is RANDOM",
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


idatFileName = "../../../AUDIOLIB/TISP_mute/TISP_mute_idat.c"

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
