from __future__ import print_function
from subprocess import call
from sys import platform

import sys
import numpy as np
import csv
import re

sys.path.append("../../../common")
import TISP_utils
import file_io
import os
import matplotlib.pyplot as plt
import json
import AUDIOLIB_channelMappings

# Dictionary to map test types to (f_start, f_end) tuples
FREQUENCY_RANGES = {
    "SWEEP": (1000.0, 10000.0),  # SWEEP: from 2000 Hz to 20000 Hz
    "SINE": None,  # SINE: use frequency from CSV
    "STATIC": None,  # STATIC: use frequency from CSV
    # "RANDOM": None,            # Optional: for completeness, if RANDOM test cases exist
}


def get_channel_layout(numChannels):
    """Define the speaker configuration for a given number of channels."""
    base_layout = [
        "FRONT_LEFT_TREBBLE",
        "FRONT_RIGHT_TREBBLE",
        "FRONT_CENTER",
        "LOW_FREQUENCY_EFFECTS",
        "SIDE_LEFT",
        "SIDE_RIGHT",
        "REAR_LEFT",
        "REAR_RIGHT",
        "FRONT_LEFT_MID",
        "FRONT_RIGHT_MID",
        "TOP_FRONT_LEFT",
        "TOP_FRONT_RIGHT",
        "FRONT_LEFT_WOOFER",
        "FRONT_RIGHT_WOOFER",
        "TOP_REAR_LEFT",
        "TOP_REAR_RIGHT",
    ]
    full_layout = (base_layout * (numChannels // len(base_layout) + 1))[:numChannels]
    return full_layout


def load_channel_layouts(json_file_path="channel_layouts.json"):
    """Load channel layouts from a JSON file."""
    try:
        with open(json_file_path, "r") as f:
            data = json.load(f)
        return data.get("layouts", {})
    except (FileNotFoundError, json.JSONDecodeError) as e:
        print(f"Error loading channel_layouts.json: {e}. Using default layout.")
        return {}


def generate_sine(
    num_samples, sampling_rate, num_frames, channel, frequency, amplitude, dtype
):
    """Generate a continuous sine wave for all samples of a given channel.

    Args:
        num_samples (int): Number of samples per frame.
        sampling_rate (int): Sampling rate in Hz.
        num_frames (int): Number of numExecReps.
        channel (int): Channel index (for phase offset if needed).
        frequency (float, optional): Frequency of the sine wave in Hz. Defaults to 2000.0 Hz.
        amplitude (float, optional): Amplitude of the sine wave. Defaults to 1.0.

    Returns:
        numpy.ndarray: Array of sine wave samples, shape (num_samples * num_frames,).
    """
    total_samples = num_samples * num_frames
    t = np.linspace(0, total_samples / sampling_rate, total_samples, endpoint=False)
    sine_wave = amplitude * np.sin(2 * np.pi * frequency * t)
    return sine_wave.astype(dtype)


def generate_sweep(
    num_samples, sampling_rate, num_frames, channel, amplitude, f_start, f_end, dtype
):
    """Generate a linear frequency sweep for all num_frames of a given channel.

    Args:
        num_samples (int): Number of samples per frame.
        sampling_rate (int): Sampling rate in Hz.
        num_frames (int): Number of frames.
        channel (int): Channel index (for phase offset).
        amplitude (float): Amplitude of the sweep signal.
        f_start (float): Starting frequency in Hz.
        f_end (float, optional): Ending frequency in Hz. Defaults to f_start * 10.

    Returns:
        numpy.ndarray: Array of sweep signal samples, shape (num_samples * num_frames,).
    """
    total_samples = num_samples * num_frames
    duration = total_samples / sampling_rate  # Total duration in seconds
    t = np.linspace(0, duration, total_samples, endpoint=False)

    # Linear sweep: f(t) = f_start + (f_end - f_start)/duration * t
    k = (f_end - f_start) / duration
    phase = 2 * np.pi * (f_start * t + 0.5 * k * t**2)
    phase_offset = channel * np.pi / 4  # 45-degree offset per channel
    sweep_signal = amplitude * np.sin(phase + phase_offset)

    return sweep_signal.astype(dtype)


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
    fader = currPrm["fader"]
    strideInElements = currPrm["strideInElements"]
    strideOutElements = currPrm["strideOutElements"]
    testType = currPrm["testType"]
    sideGainFactor = currPrm["sideGainFactor"]
    lfeBalanceGainFactor = currPrm["lfeBalanceGainFactor"]
    lfeFaderGainFactor = currPrm["lfeFaderGainFactor"]
    lfeBalanceMode = currPrm["lfeBalanceMode"]
    lfeFaderMode = currPrm["lfeFaderMode"]
    blockWidth = currPrm["blockWidth"]
    blockHeight = currPrm["blockHeight"]
    isInterleave = 1 if currPrm["isInterleave"] else 0  # Convert boolean to 1 or 0
    AUDIOLIB_dType = TISP_utils.resolve_AUDIOLIB_dType(dType)

    in_ref = "staticRefIn" + str(testId)
    out_ref = "staticRefOut" + str(testId)

    if testType in ["STATIC", "SINE", "SWEEP"]:
        in_ref = "staticRefInCase" + testId
        channelConfig_ref = "channelConfigCase" + testId
        out_ref = "staticRefOutCase" + testId
    else:
        in_ref = "NULL"
        out_ref = "NULL"

    testParamsFile.write(
        "#if (defined(ALL_TEST_CASES) || (TEST_CASE == %d))\n" % (int(testId))
    )

    if testType in ["SINE", "SWEEP"]:
        testType = "STATIC"

    # For interleaved data, set strideOutElements to numChannels
    if isInterleave:
        strideOutElements = numChannels

    idatFile.write("{\n")
    idatFile.write("%s, // Test Pattern \n" % (testType))
    idatFile.write("%s, // staticIn \n" % (in_ref))
    idatFile.write("%s, // staticOut \n" % (out_ref))
    idatFile.write("%s, // Sample Data Type \n" % (AUDIOLIB_dType))
    idatFile.write("%d, // Samples per channel \n" % (numSamples))
    idatFile.write("%d, // Number of Channels \n" % (numChannels))
    idatFile.write("%s, // Balance \n" % (balance))
    idatFile.write("%s, // Fader \n" % (fader))
    idatFile.write("%s, // Side Gain Factor \n" % (sideGainFactor))
    idatFile.write("%s, // LFE Balance Gain Factor \n" % (lfeBalanceGainFactor))
    idatFile.write("%s, // LFE Fader Gain Factor \n" % (lfeFaderGainFactor))
    idatFile.write("%d, // LFE Balance Mode \n" % (lfeBalanceMode))
    idatFile.write("%d, // LFE Fader Mode \n" % (lfeFaderMode))
    idatFile.write("%s, // Channel Config \n" % (channelConfig_ref))
    idatFile.write("%d, // isInterleave\n" % (isInterleave))
    idatFile.write(
        "AUDIOLIB_CALC_STRIDE(%s * sizeof(%s), AUDIOLIB_ALIGN_SHIFT_64BYTES), // strideInElements \n"
        % ((strideInElements), dType)
    )
    idatFile.write(
        "AUDIOLIB_CALC_STRIDE(%s * sizeof(%s), AUDIOLIB_ALIGN_SHIFT_64BYTES), // strideOutElements \n"
        % ((strideOutElements), dType)
    )
    idatFile.write("%d, //blockWidth\n" % int(blockWidth))
    idatFile.write("%d, //blockHeight\n" % int(blockHeight))
    idatFile.write("%d, // test ID\n" % int(testId))
    idatFile.write("},\n")
    idatFile.write("#endif\n")

    idatFile.write("\n\n")


def gen_idat_file(idatFile, testCases):
    """Generate the idat file

    Args:
        param1 (file object): Input file object to idat.c file
        param2 (dict object): Dict object with test case parameter

    """

    fIn = open("../../../common/ti_lice_header_idat.txt", "r")
    lines = fIn.readlines()
    fIn.close()
    idatFile.writelines(lines)
    includeString = '#include "TISP_balanceFader_superNode_idat.h"'
    idatFile.writelines(includeString)
    idatFile.write("\n")
    idatFile.write("/*This file is autogenerated*/\n")
    idatFile.write("/*Please do not edit*/\n")
    idatFile.write("\n\n")
    # generate ifdefs for header files
    for testCase in testCases:
        if testCase["testType"] in ["STATIC", "SINE", "SWEEP"]:
            TISP_utils.gen_ifdefs(idatFile, testCase)

    # generate parameters for test cases
    idatFile.write("static TISP_balanceFader_superNode_testParams_t testParams[] =  ")
    idatFile.write("{\n")

    for testCase in testCases:
        gen_testCaseParams(idatFile, testCase)

    idatFile.write("};\n\n")
    fIn = open("test_param_function.txt", "r")
    lines = fIn.readlines()

    idatFile.writelines(lines)


def process_test_case(testCase):
    """Convert str to int for necessary parameters."""
    testCase["testId"] = str(testCase["ID"])
    testCase["numSamples"] = int(testCase["numSamples"])
    testCase["balance"] = float(testCase.get("balance", 0.0))
    testCase["fader"] = float(testCase.get("fader", 0.0))
    testCase["samplingRate"] = int(testCase["samplingRate"])
    testCase["strideInElements"] = int(testCase["strideInElements"])
    testCase["strideOutElements"] = int(testCase["strideOutElements"])
    testCase["plotSignals"] = testCase.get("plotSignals", "FALSE").upper() == "TRUE"
    testCase["waveFiles"] = testCase.get("waveFiles", "FALSE").upper() == "TRUE"
    testCase["amplitude"] = float(testCase.get("amplitude", 1.0))
    testCase["frequency"] = float(testCase.get("frequency", 2000.0))
    testCase["lfeBalanceMode"] = testCase.get("lfeBalanceMode", "unaffected")
    testCase["lfeFaderMode"] = testCase.get("lfeFaderMode", "unaffected")
    testCase["isInterleave"] = testCase.get("isInterleave", "FALSE").upper() == "TRUE"
    testCase["sideGainFactor"] = float(testCase.get("sideGainFactor", 0.7))
    testCase["lfeBalanceGainFactor"] = float(testCase.get("lfeBalanceGainFactor", 0.7))
    testCase["lfeFaderGainFactor"] = float(testCase.get("lfeFaderGainFactor", 0.7))
    testCase["blockHeight"] = int(testCase["blockHeight"])
    testCase["blockWidth"] = int(testCase["blockWidth"])

    lfe_balance_mode_str = testCase.get("lfeBalanceMode", "unaffected").upper()
    lfe_balance_mode_key = f"AUDIOLIB_LFE_BALANCE_{lfe_balance_mode_str}"
    if (
        lfe_balance_mode_key
        not in AUDIOLIB_channelMappings.AUDIOLIB_LFE_BALANCE_MODE_MAP
    ):
        print(
            f"Warning: Test ID {testCase['ID']} has invalid lfeBalanceMode '{lfe_balance_mode_str}'. Defaulting to 'UNAFFECTED'."
        )
        lfe_balance_mode_key = "AUDIOLIB_LFE_BALANCE_UNAFFECTED"
    testCase["lfeBalanceMode"] = AUDIOLIB_channelMappings.AUDIOLIB_LFE_BALANCE_MODE_MAP[
        lfe_balance_mode_key
    ]

    lfe_fader_mode_str = testCase.get("lfeFaderMode", "unaffected").upper()
    lfe_fader_mode_key = f"AUDIOLIB_LFE_FADER_{lfe_fader_mode_str}"
    if lfe_fader_mode_key not in AUDIOLIB_channelMappings.AUDIOLIB_LFE_FADER_MODE_MAP:
        print(
            f"Warning: Test ID {testCase['ID']} has invalid lfeFaderMode '{lfe_fader_mode_str}'. Defaulting to 'UNAFFECTED'."
        )
        lfe_fader_mode_key = "AUDIOLIB_LFE_FADER_UNAFFECTED"
    testCase["lfeFaderMode"] = AUDIOLIB_channelMappings.AUDIOLIB_LFE_FADER_MODE_MAP[
        lfe_fader_mode_key
    ]

    # Load channel layouts from JSON
    channel_layouts = load_channel_layouts()
    layout_name = testCase.get("channelLayoutName", "").strip()
    if layout_name and layout_name in channel_layouts:
        layout = channel_layouts[layout_name]
        testCase["numChannels"] = layout["numChannels"]
        speaker_layout_names = layout["speakers"]
        testCase["channelFrequencies"] = layout["frequencies"]
        if len(speaker_layout_names) != testCase["numChannels"]:
            print(
                f"Warning: Test ID {testCase['ID']} channelLayoutName '{layout_name}' has {len(speaker_layout_names)} speakers, expected {testCase['numChannels']}. Using default layout."
            )
            speaker_layout_names = get_channel_layout(testCase["numChannels"])
            testCase["channelFrequencies"] = {
                name: testCase["frequency"] for name in speaker_layout_names
            }
    else:
        if layout_name:
            print(
                f"Warning: Test ID {testCase['ID']} channelLayoutName '{layout_name}' not found in channel_layouts.json. Using default layout."
            )
        speaker_layout_names = get_channel_layout(int(testCase.get("numChannels", 6)))
        testCase["numChannels"] = len(speaker_layout_names)
        testCase["channelFrequencies"] = {
            name: testCase["frequency"] for name in speaker_layout_names
        }
    testCase["channelLayout"] = speaker_layout_names

    test_type = testCase["testType"]
    if test_type in FREQUENCY_RANGES and FREQUENCY_RANGES[test_type] is not None:
        testCase["f_start"], testCase["f_end"] = FREQUENCY_RANGES[test_type]
    else:
        testCase["f_start"] = testCase["frequency"]
        testCase["f_end"] = testCase["frequency"]

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
    balance = testCase["balance"]
    fader = testCase["fader"]
    lfeBalanceMode = testCase["lfeBalanceMode"]
    lfeFaderMode = testCase["lfeFaderMode"]
    numSamples = testCase["numSamples"]
    samplingRate = testCase["samplingRate"]
    testType = testCase["testType"]
    amplitude = testCase["amplitude"]
    frequency = testCase["frequency"]
    f_start = testCase["f_start"]
    f_end = testCase["f_end"]
    plotSignals = testCase["plotSignals"]
    waveFiles = testCase["waveFiles"]
    numChannels = testCase["numChannels"]
    dType = testCase["dType"]
    isInterleave = testCase["isInterleave"]
    speaker_layout_names = testCase["channelLayout"]
    channel_frequencies = testCase["channelFrequencies"]
    sideGainFactor = testCase["sideGainFactor"]
    lfeBalanceGainFactor = testCase["lfeBalanceGainFactor"]
    lfeFaderGainFactor = testCase["lfeFaderGainFactor"]
    blockWidth = testCase["blockWidth"]
    blockHeight = testCase["blockHeight"]

    CMAP = AUDIOLIB_channelMappings.AUDIOLIB_CHANNEL_MAP
    LFE_BALANCE_MAP = AUDIOLIB_channelMappings.AUDIOLIB_LFE_BALANCE_MODE_MAP
    LFE_FADER_MAP = AUDIOLIB_channelMappings.AUDIOLIB_LFE_FADER_MODE_MAP

    # Map dType to NumPy dtype
    dtype = np.float32 if dType == "float" else np.float64

    samplesPerFrame = numSamples * numChannels
    [minVal, maxVal] = [-10, 10]

    # --- 1. Get the custom channel layout and convert to integer array ---
    channel_config_integers = [
        CMAP[f"AUDIOLIB_CHANNEL_{name}"] for name in speaker_layout_names
    ]

    # --- 2. Calculate Fundamental Gain Components ---
    balance_angle = (np.pi / 4.0) * (balance + 1)
    targetGainL = np.cos(balance_angle)
    targetGainR = np.sin(balance_angle)

    fader_angle = (np.pi / 4.0) * (fader + 1)
    frontGain = np.sin(fader_angle)
    backGain = np.cos(fader_angle)
    sideGain = (
        sideGainFactor * backGain + (1 - sideGainFactor) * frontGain
        if fader < 0
        else backGain
    )
    topMiddleGain = 0.5 * (frontGain + backGain)

    # --- 3. Calculate Final Gain for Each Channel in the Layout ---
    final_channel_gains = np.zeros(numChannels, dtype=dtype)
    for i, channel_name in enumerate(speaker_layout_names):
        channel_enum = CMAP[f"AUDIOLIB_CHANNEL_{channel_name}"]
        gain = 0.0

        front_left_channels = [
            CMAP["AUDIOLIB_CHANNEL_FRONT_LEFT"],
            CMAP["AUDIOLIB_CHANNEL_FRONT_LEFT_TREBBLE"],
            CMAP["AUDIOLIB_CHANNEL_FRONT_LEFT_MID"],
            CMAP["AUDIOLIB_CHANNEL_FRONT_LEFT_WOOFER"],
        ]
        front_right_channels = [
            CMAP["AUDIOLIB_CHANNEL_FRONT_RIGHT"],
            CMAP["AUDIOLIB_CHANNEL_FRONT_RIGHT_TREBBLE"],
            CMAP["AUDIOLIB_CHANNEL_FRONT_RIGHT_MID"],
            CMAP["AUDIOLIB_CHANNEL_FRONT_RIGHT_WOOFER"],
        ]

        if channel_enum in front_left_channels:
            gain = targetGainL * frontGain
        elif channel_enum in front_right_channels:
            gain = targetGainR * frontGain
        elif channel_enum == CMAP["AUDIOLIB_CHANNEL_FRONT_CENTER"]:
            gain = 1.0 * frontGain
        elif channel_enum == CMAP["AUDIOLIB_CHANNEL_LOW_FREQUENCY_EFFECTS"]:
            lfeBalanceGain = 1.0
            if lfeBalanceMode == LFE_BALANCE_MAP["AUDIOLIB_LFE_BALANCE_PARTIAL"]:
                lfeBalanceGain = lfeBalanceGainFactor + (
                    1 - lfeBalanceGainFactor
                ) * np.cos(2 * balance_angle)
            elif lfeBalanceMode == LFE_BALANCE_MAP["AUDIOLIB_LFE_BALANCE_FULL"]:
                lfeBalanceGain = 0.5 * (targetGainL + targetGainR)

            lfeFaderGain = 1.0
            if lfeFaderMode == LFE_FADER_MAP["AUDIOLIB_LFE_FADER_AVERAGE"]:
                lfeFaderGain = 0.5 * (frontGain + backGain)
            elif lfeFaderMode == LFE_FADER_MAP["AUDIOLIB_LFE_FADER_POSITION_BASED"]:
                lfeFaderGain = (
                    1 - lfeFaderGainFactor
                ) * frontGain + lfeFaderGainFactor * backGain
            elif lfeFaderMode == LFE_FADER_MAP["AUDIOLIB_LFE_FADER_FULL"]:
                lfeFaderGain = backGain
            gain = lfeBalanceGain * lfeFaderGain
        elif channel_enum == CMAP["AUDIOLIB_CHANNEL_SIDE_LEFT"]:
            gain = targetGainL * sideGain
        elif channel_enum == CMAP["AUDIOLIB_CHANNEL_SIDE_RIGHT"]:
            gain = targetGainR * sideGain
        elif channel_enum == CMAP["AUDIOLIB_CHANNEL_REAR_LEFT"]:
            gain = targetGainL * backGain
        elif channel_enum == CMAP["AUDIOLIB_CHANNEL_REAR_RIGHT"]:
            gain = targetGainR * backGain
        elif channel_enum == CMAP["AUDIOLIB_CHANNEL_TOP_FRONT_LEFT"]:
            gain = targetGainL * frontGain
        elif channel_enum == CMAP["AUDIOLIB_CHANNEL_TOP_FRONT_RIGHT"]:
            gain = targetGainR * frontGain
        elif channel_enum == CMAP["AUDIOLIB_CHANNEL_TOP_MIDDLE_LEFT"]:
            gain = targetGainL * topMiddleGain
        elif channel_enum == CMAP["AUDIOLIB_CHANNEL_TOP_MIDDLE_RIGHT"]:
            gain = targetGainR * topMiddleGain
        elif channel_enum == CMAP["AUDIOLIB_CHANNEL_TOP_REAR_LEFT"]:
            gain = targetGainL * backGain
        elif channel_enum == CMAP["AUDIOLIB_CHANNEL_TOP_REAR_RIGHT"]:
            gain = targetGainR * backGain
        elif channel_enum == CMAP["AUDIOLIB_CHANNEL_OVERHEAD"]:
            gain = 1.0 * topMiddleGain

        final_channel_gains[i] = gain

    # --- 4. Generate Input and Output Signals ---
    in0 = np.zeros((numChannels, numSamples), dtype=dtype)
    if testType == "SINE":
        for i, channel_name in enumerate(speaker_layout_names):
            channel_enum = CMAP[f"AUDIOLIB_CHANNEL_{channel_name}"]
            # Assign channel-specific frequency or default
            channel_freq = channel_frequencies.get(channel_enum, frequency + i * 50)
            in0[i, :] = generate_sine(
                numSamples, samplingRate, 1, i, channel_freq, amplitude, dtype
            )
    elif testType == "SWEEP":
        for i in range(numChannels):
            in0[i, :] = generate_sweep(
                numSamples, samplingRate, 1, i, amplitude, f_start, f_end, dtype
            )
    else:  # STATIC
        in0 = np.random.uniform(
            low=minVal, high=maxVal, size=(numChannels, numSamples)
        ).astype(dtype)

    in1 = final_channel_gains  # Gains

    if not isInterleave:
        # Non-interleaved: shape (numChannels, numSamples)
        out = in0 * in1[:, np.newaxis]
        in_list = in0.flatten()  # 2D array for non-interleaved
        out_list = out.flatten()
        pIn_interleaved = in0.T.flatten()  # For WAV and plots
        pOut_interleaved = out.T.flatten()
    else:
        # Interleaved: shape (numSamples * numChannels,)
        in0_transposed = in0.T  # Shape (numSamples, numChannels)
        out = in0_transposed * in1
        in_list = in0_transposed.flatten()  # 1D interleaved
        out_list = out.flatten()
        pIn_interleaved = in_list  # Already interleaved
        pOut_interleaved = out_list

    # Generate plots if plotSignals is TRUE
    if plotSignals:
        file_io.plot_signal_pair(
            final_channel_gains,
            numSamples,
            numChannels,
            testId,
            "gain",
            channel=None,
            color="brown",
            speaker_layout=speaker_layout_names,
        )
        for i in range(numChannels):
            file_io.plot_signal_pair(
                pIn_interleaved,
                numSamples,
                numChannels,
                testId,
                "input",
                channel=i,
                color="blue",
                speaker_layout=speaker_layout_names,
            )
            file_io.plot_signal_pair(
                pOut_interleaved,
                numSamples,
                numChannels,
                testId,
                "output",
                channel=i,
                color="green",
                speaker_layout=speaker_layout_names,
            )

    # Generate WAV files if waveFiles is TRUE
    if waveFiles:
        file_io.write_wav_files(pIn_interleaved, pOut_interleaved, testCase)

    # gain_list  = final_channel_gains.flatten().tolist() #gain values

    # Write header file with flattened 1D input, output, and gains
    headerFileName = "staticRefCase" + str(testId) + ".h"
    file_io.write_header_file(
        in_list,
        out_list,
        testCase=testCase,
        headerFileName=headerFileName,
        channel_config_integers=channel_config_integers,
    )

    return [np.min(out), np.max(out)]


# CSV file with paratmeters for all test cases
testCasesCsvFile = "test_cases_list.csv"
testIdList = []  # list of test IDs that we want to generate test data
testIdList += range(0, 200)
# testIdList = [18]

myArgs = TISP_utils.getCmdLineArgs()


def gen_test_case(testCase):
    if testCase["testType"] in ["STATIC", "SINE", "SWEEP"]:
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

    for i in range(len(testCases)):
        testCases[i] = process_test_case(testCases[i])

    for testCase in testCases:
        if myArgs.allCases == True:
            gen_test_case(testCase)
        elif int(testCase["ID"]) in testIdList:
            gen_test_case(testCase)


idatFileName = "../../../../superNode/i2d_to_o2d/TISP_balanceFader_superNode/TISP_balanceFader_superNode_idat.cpp"


##########################################################################
# Generate idat.cpp file by parsing all the test cases in the CSV file # #
##########################################################################


with open(idatFileName, "w") as idatFile:
    gen_idat_file(idatFile, testCases)


# if platform == "linux" or platform == "linux2":
#     call(["clang-format", "-style=file", "-i", idatFileName])
#     call(["indent", "-nut", "-i3", "-c55", "-l200", idatFileName])
#     call(["rm", idatFileName + "~"])  # remove backup file


print("Idat file generation completed")
