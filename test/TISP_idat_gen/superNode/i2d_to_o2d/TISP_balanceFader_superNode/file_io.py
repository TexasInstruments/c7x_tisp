from __future__ import print_function
from subprocess import call
from sys import platform
import sys
import numpy as np
import csv
import os
from scipy.io import wavfile
import matplotlib.pyplot as plt


def write_attributes(f, attributes, stringToWrite):
    """Writes platform-specific C attributes for variable declarations."""
    f.write("#ifdef WIN32\n")
    f.write(stringToWrite)
    f.write("#else\n")
    f.write(attributes + stringToWrite)
    f.write("#endif\n")
    f.write("{\n")


def write_wav_files(pIn, pOut, testCase):
    """Write multi-channel WAV files from single input/output arrays for a single frame."""
    testId = testCase["ID"]
    numChannels = testCase["numChannels"]
    samplingRate = testCase["samplingRate"]
    numSamples = testCase["numSamples"]

    base_output_dir = "../../../../superNode/i2d_to_o2d/TISP_balanceFader_superNode/test_data/wave_files"
    output_dir = os.path.join(base_output_dir, f"Testcase_{testId}")
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    # Reshape the flat arrays into (numSamples, numChannels) for WAV
    in_reshaped = pIn.reshape((numSamples, numChannels))
    out_reshaped = pOut.reshape((numSamples, numChannels))

    # Scale signals to 16-bit PCM range
    max_amplitude = max(np.max(np.abs(pIn)), np.max(np.abs(pOut)), 1.0)
    in_scaled = (in_reshaped * 32767.0 / max_amplitude).astype(np.int16)
    out_scaled = (out_reshaped * 32767.0 / max_amplitude).astype(np.int16)

    wavfile.write(
        os.path.join(output_dir, f"input_case{testId}.wav"), samplingRate, in_scaled
    )
    wavfile.write(
        os.path.join(output_dir, f"output_case{testId}.wav"), samplingRate, out_scaled
    )


def plot_signal_pair(
    pData,
    num_samples,
    num_channels,
    test_id,
    signal_type,
    channel=0,
    color="blue",
    speaker_layout=None,
):
    """Plot a specific channel from a single, interleaved data array or gain array."""
    base_output_dir = "../../../../superNode/i2d_to_o2d/TISP_balanceFader_superNode/test_data/plot_diagrams"
    output_dir = os.path.join(base_output_dir, f"Testcase_{test_id}")
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    plt.figure(figsize=(10, 5))

    if signal_type == "gain":
        signal_to_plot = pData
        plt.plot(
            signal_to_plot,
            "o-",
            label=f"Gain Values (Test Case {test_id})",
            color=color,
        )
        plt.xlabel("Channel Index")
        plt.ylabel("Gain")
        plt.title(f"Channel Gains (Test Case {test_id})")
        if speaker_layout:
            plt.xticks(
                ticks=range(len(speaker_layout)),
                labels=speaker_layout,
                rotation=45,
                ha="right",
            )
    else:
        if channel is None or channel < 0 or channel >= num_channels:
            raise ValueError(
                f"Invalid channel index {channel} for {signal_type} plotting"
            )

        speaker_name = (
            speaker_layout[channel]
            if speaker_layout and channel < len(speaker_layout)
            else f"Channel {channel}"
        )
        plot_title = f"{signal_type.capitalize()} Signal for {speaker_name} (Test Case {test_id})"
        label_text = f"{signal_type.capitalize()} for {speaker_name}"

        signal_to_plot = pData[channel::num_channels][:num_samples]
        plt.plot(signal_to_plot, label=label_text, color=color)
        plt.xlabel("Sample Index")
        plt.ylabel("Amplitude")
        plt.title(plot_title)

        # Set a fixed y-axis limit to prevent autoscaling on near-zero signals
        plt.ylim([-1.1, 1.1])

    plt.legend()
    plt.grid(True)
    plt.tight_layout()
    plt.savefig(
        os.path.join(
            output_dir,
            f'{signal_type}_case{test_id}_ch{channel if channel is not None else "gains"}.png',
        )
    )
    plt.close()


def write_header_file(pIn, pOut, testCase, headerFileName, channel_config_integers):
    """Write header file with input/output arrays, channel config, and gains, with comments indicating interleaved or non-interleaved format."""
    testId = testCase["ID"]
    numSamples = testCase["numSamples"]
    numChannels = testCase["numChannels"]
    blockHeight = testCase["blockHeight"]
    blockWidth = testCase["blockWidth"]
    dType = testCase["dType"]
    isInterleave = testCase["isInterleave"]
    testType = testCase["testType"]
    frequency = testCase.get("frequency", 2000.0)
    f_start = testCase.get("f_start", frequency)
    f_end = testCase.get("f_end", frequency)
    totalLength = numSamples * numChannels

    # Validate dType and set format string and C datatype
    if dType not in ["float", "double"]:
        print(
            f"Warning: Invalid dType '{dType}' for Test Case {testId}. Defaulting to float."
        )
        dType = "float"
    format_str = "{x:.7f}f" if dType == "float" else "{x:.15f}"
    c_type = "float" if dType == "float" else "double"

    attributes = '__attribute__((section(".staticData"))) static '
    outputDir = (
        "../../../../superNode/i2d_to_o2d/TISP_balanceFader_superNode/test_data/"
    )

    if not os.path.exists(outputDir):
        os.makedirs(outputDir)

    with open(os.path.join(outputDir, headerFileName), "w") as f:
        f.write(f"/* Test Case {testId} for AUDIOLIB_balanceFader\n")
        f.write(f" * Input: {numChannels} channels, {numSamples} samples per channel\n")
        f.write(f" * Data Type: {c_type}\n")
        f.write(f" * Test Type: {testType}\n")
        f.write(
            f" * Frequency: {f_start}Hz to {f_end}Hz\n"
            if testType == "SWEEP"
            else f" * Frequency: {frequency}Hz\n"
        )
        f.write(f" */\n\n")

        # Write Channel Config Array
        write_attributes(
            f, attributes, f"int32_t channelConfigCase{testId}[{numChannels}] =\n"
        )
        f.write("    " + ", ".join(map(str, channel_config_integers)) + "\n")
        f.write("};\n\n")

        # # Write Gains Array
        # write_attributes(
        #     f,
        #     attributes,
        #     f"{c_type} staticGainsCase{testId}[{numChannels}] =\n"
        # )
        # f.write("    " + ", ".join(format_str.format(x=x) for x in final_channel_gains) + "\n")
        # f.write("};\n\n")

        # Write Input Array with Format Comment
        if isInterleave:
            f.write(
                f"/* Interleaved format: [ch0_s0, ch1_s0, ..., ch{numChannels-1}_s0, ch0_s1, ...] */\n"
            )
        else:
            f.write(
                f"/* Non-interleaved format, flattened: [ch0_s0, ..., ch0_s{numSamples-1}, ch1_s0, ..., ch{numChannels-1}_s{numSamples-1}] */\n"
            )
        write_attributes(
            f, attributes, f"{c_type} staticRefInCase{testId}[{totalLength}] =\n"
        )
        for i in range(0, totalLength, 8):
            f.write(
                "    "
                + ", ".join(
                    format_str.format(x=x) for x in pIn[i : min(i + 8, totalLength)]
                )
                + ",\n"
            )
        f.write("};\n\n")

        # Write Output Array with Format Comment
        if isInterleave:
            f.write(
                f"/* Interleaved format: [ch0_s0, ch1_s0, ..., ch{numChannels-1}_s0, ch0_s1, ...] */\n"
            )
        else:
            f.write(
                f"/* Non-interleaved format, flattened: [ch0_s0, ..., ch0_s{numSamples-1}, ch1_s0, ..., ch{numChannels-1}_s{numSamples-1}] */\n"
            )
        write_attributes(
            f, attributes, f"{c_type} staticRefOutCase{testId}[{totalLength}] =\n"
        )
        for i in range(0, totalLength, 8):
            f.write(
                "    "
                + ", ".join(
                    format_str.format(x=x) for x in pOut[i : min(i + 8, totalLength)]
                )
                + ",\n"
            )
        f.write("};\n\n")
        f.close()

    # if platform == "linux" or platform == "linux2":
    #     call(
    #         [
    #             "indent",
    #             "-nut",
    #             "-i3",
    #             "-l100",
    #             os.path.join(outputDir, headerFileName),
    #         ]
    #     )
    #     call(["rm", os.path.join(outputDir, headerFileName + "~")])
