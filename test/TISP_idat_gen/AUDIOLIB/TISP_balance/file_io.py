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
    f.write("#ifdef WIN32\n")
    f.write(stringToWrite)
    f.write("#else\n")
    f.write(attributes + stringToWrite)
    f.write("#endif\n")
    f.write("{\n")


def write_wav_files(
    in_left, in_right, out_left, out_right, gain_left, gain_right, testCase, frames
):
    """Write multi-channel WAV files for input and output signals, and mono WAV files for gains.

    Args:
        in_left (numpy.ndarray): Left channel input data [frames, numSamples * numChannels]
        in_right (numpy.ndarray): Right channel input data [frames, numSamples * numChannels]
        out_left (numpy.ndarray): Left channel output data [frames, numSamples * numChannels]
        out_right (numpy.ndarray): Right channel output data [frames, numSamples * numChannels]
        gain_left (numpy.ndarray): Left channel gain data [numSamples * frames]
        gain_right (numpy.ndarray): Right channel gain data [numSamples * frames]
        testCase (dict): Dictionary with test case parameters
        frames (int): Number of frames
    """
    testId = testCase["ID"]
    numChannels = testCase["numChannels"]
    samplingRate = testCase["samplingRate"]
    numSamples = testCase["numSamples"]
    total_samples = numSamples * frames

    base_output_dir = "../../../AUDIOLIB/TISP_balance/test_data/wave_files"
    output_dir = os.path.join(base_output_dir, f"Testcase_{testId}")
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    # Prepare multi-channel arrays
    in_left_channels = np.zeros((total_samples, numChannels), dtype=np.int16)
    in_right_channels = np.zeros((total_samples, numChannels), dtype=np.int16)
    out_left_channels = np.zeros((total_samples, numChannels), dtype=np.int16)
    out_right_channels = np.zeros((total_samples, numChannels), dtype=np.int16)

    # Scale signals to 16-bit PCM range
    max_amplitude = max(np.max(np.abs([in_left, in_right, out_left, out_right])), 1.0)
    gain_scale_factor = 32767.0
    for channel in range(numChannels):
        for frame in range(frames):
            start_idx = channel * numSamples
            end_idx = (channel + 1) * numSamples
            sample_start = frame * numSamples
            sample_end = (frame + 1) * numSamples
            in_left_channels[sample_start:sample_end, channel] = (
                in_left[frame, start_idx:end_idx] * 32767.0 / max_amplitude
            ).astype(np.int16)
            in_right_channels[sample_start:sample_end, channel] = (
                in_right[frame, start_idx:end_idx] * 32767.0 / max_amplitude
            ).astype(np.int16)
            out_left_channels[sample_start:sample_end, channel] = (
                out_left[frame, start_idx:end_idx] * 32767.0 / max_amplitude
            ).astype(np.int16)
            out_right_channels[sample_start:sample_end, channel] = (
                out_right[frame, start_idx:end_idx] * 32767.0 / max_amplitude
            ).astype(np.int16)

    # gain_left_scaled = (gain_left[:total_samples] * gain_scale_factor).astype(np.int16)
    # gain_right_scaled = (gain_right[:total_samples] * gain_scale_factor).astype(np.int16)

    # Write multi-channel WAV files
    wavfile.write(
        os.path.join(output_dir, f"inputLeft_case{testId}.wav"),
        samplingRate,
        in_left_channels,
    )
    wavfile.write(
        os.path.join(output_dir, f"inputRight_case{testId}.wav"),
        samplingRate,
        in_right_channels,
    )
    wavfile.write(
        os.path.join(output_dir, f"outputLeft_case{testId}.wav"),
        samplingRate,
        out_left_channels,
    )
    wavfile.write(
        os.path.join(output_dir, f"outputRight_case{testId}.wav"),
        samplingRate,
        out_right_channels,
    )


def plot_signal_pair(
    data_left,
    data_right,
    num_samples,
    num_channels,
    num_frames,
    test_id,
    signal_type,
    channel=None,
    colors=["blue", "violet"],
    amplitude=1.0,
    frequency=2000.0,
    test_type="STATIC",
):
    """Plot left and right signals side by side in a single PNG for comparison.

    Args:
        data_left (numpy.ndarray): Left signal data, either (numBlocks, numSamples * numChannels) or (numSamples * numBlocks,).
        data_right (numpy.ndarray): Right signal data, same shape as data_left.
        num_samples (int): Number of samples per channel per frame.
        num_channels (int): Number of channels (ignored for gain arrays).
        num_frames (int): Number of numBlocks.
        test_id (str): Test case ID for the title and subfolder.
        signal_type (str): Type of signal ('input', 'output', or 'gain').
        channel (int, optional): Channel index for input/output signals. None for gains.
        colors (list, optional): Colors for left and right plots. Defaults to ['blue', 'violet'].
        amplitude (float, optional): Amplitude of the signal (used for scaling in plots). Defaults to 1.0.
        frequency (float, optional): Frequency of the signal (for reference in title). Defaults to 2000.0.
        test_type (str, optional): Test type ('STATIC' or 'SINE'). Defaults to 'STATIC'.
    """
    base_output_dir = "../../../AUDIOLIB/TISP_balance/test_data/plot_diagrams"
    output_dir = os.path.join(base_output_dir, f"Testcase_{test_id}")
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    total_samples = num_samples * num_frames
    fig, (ax_left, ax_right) = plt.subplots(1, 2, figsize=(12, 5), sharey=True)

    if channel is not None:
        # Input/Output signals: Extract channel data across all numBlocks
        signal_left = np.zeros(total_samples, dtype=np.float32)
        signal_right = np.zeros(total_samples, dtype=np.float32)
        for frame in range(num_frames):
            startIdx = channel * num_samples
            endIdx = (channel + 1) * num_samples
            signal_left[frame * num_samples : (frame + 1) * num_samples] = data_left[
                frame, startIdx:endIdx
            ]
            signal_right[frame * num_samples : (frame + 1) * num_samples] = data_right[
                frame, startIdx:endIdx
            ]
        label_left = f"{signal_type.capitalize()} Left Channel {channel}"
        label_right = f"{signal_type.capitalize()} Right Channel {channel}"
        filename = f"{signal_type}_case{test_id}_ch{channel}.png"
    else:
        # Gain signals: Use all samples
        signal_left = data_left[:total_samples]
        signal_right = data_right[:total_samples]
        label_left = f"{signal_type.capitalize()} Left"
        label_right = f"{signal_type.capitalize()} Right"
        filename = f"{signal_type}_case{test_id}.png"

    # Plot left signal
    ax_left.plot(range(total_samples), signal_left, label=label_left, color=colors[0])
    ax_left.set_xlabel("Sample Index")
    ax_left.set_ylabel("Amplitude")
    ax_left.set_title(f"{label_left} (Test Case {test_id})")
    ax_left.legend()
    ax_left.grid(True)

    # Plot right signal
    ax_right.plot(
        range(total_samples), signal_right, label=label_right, color=colors[1]
    )
    ax_right.set_xlabel("Sample Index")
    ax_right.set_title(f"{label_right} (Test Case {test_id})")
    ax_right.legend()
    ax_right.grid(True)

    # Adjust layout and save
    plt.suptitle(
        f"{signal_type.capitalize()} Signals for Test Case {test_id} ({num_frames} Frames, Test Type: {test_type}, Amplitude: {amplitude}, Frequency: {frequency} Hz)",
        y=1.05,
    )
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, filename), bbox_inches="tight")
    plt.close()


def write_header_file(
    in_left,
    in_right,
    out_left,
    out_right,
    gain_left,
    gain_right,
    testCase,
    headerFileName,
    frames,
):
    """Write header file with flattened 1D left and right channel inputs, outputs, and gains, organized by frame.

    Args:
        in_left (list): 1D list of left channel input data [frames * numSamples * numChannels]
        in_right (list): 1D list of right channel input data [frames * numSamples * numChannels]
        out_left (list): 1D list of left channel output data [frames * numSamples * numChannels]
        out_right (list): 1D list of right channel output data [frames * numSamples * numChannels]
        gain_left (list): 1D list of left channel gain data [numSamples * frames]
        gain_right (list): 1D list of right channel gain data [numSamples * frames]
        testCase (dict): Dictionary with test case parameters
        headerFileName (str): Name of the header file to write
        frames (int): Number of frames
    """
    testId = testCase["ID"]
    numSamples = testCase["numSamples"]
    numChannels = testCase["numChannels"]
    dType = testCase["dType"]
    totalLength = numSamples * numChannels * frames

    attributes = '__attribute__((section(".staticData"))) static '
    outputDir = "../../../AUDIOLIB/TISP_balance/test_data/"

    if not os.path.exists(outputDir):
        os.makedirs(outputDir)

    with open(os.path.join(outputDir, headerFileName), "w") as f:
        # Write left channel input
        write_attributes(
            f, attributes, f"{dType} staticRefInLeftCase{testId}[{totalLength}] =\n"
        )
        f.write(
            f"    /* Frame-based input: {frames} frames, {numChannels} channels, {numSamples} samples per channel per frame */\n"
        )
        for frame in range(frames):
            f.write(f"    /* Frame {frame} */\n")
            start_idx = frame * numSamples * numChannels
            end_idx = (frame + 1) * numSamples * numChannels
            f.write(
                "    " + ", ".join(f"{x:}" for x in in_left[start_idx:end_idx]) + ",\n"
            )
        f.write("};\n\n")

        # Write right channel input
        write_attributes(
            f, attributes, f"{dType} staticRefInRightCase{testId}[{totalLength}] =\n"
        )
        f.write(
            f"    /* Frame-based input: {frames} frames, {numChannels} channels, {numSamples} samples per channel per frame */\n"
        )
        for frame in range(frames):
            f.write(f"    /* Frame {frame} */\n")
            start_idx = frame * numSamples * numChannels
            end_idx = (frame + 1) * numSamples * numChannels
            f.write(
                "    " + ", ".join(f"{x:}" for x in in_right[start_idx:end_idx]) + ",\n"
            )
        f.write("};\n\n")

        # Write left channel output
        write_attributes(
            f, attributes, f"{dType} staticRefOutLeftCase{testId}[{totalLength}] =\n"
        )
        f.write(
            f"    /* Frame-based output: {frames} frames, {numChannels} channels, {numSamples} samples per channel per frame */\n"
        )
        for frame in range(frames):
            f.write(f"    /* Frame {frame} */\n")
            start_idx = frame * numSamples * numChannels
            end_idx = (frame + 1) * numSamples * numChannels
            f.write(
                "    " + ", ".join(f"{x:}" for x in out_left[start_idx:end_idx]) + ",\n"
            )
        f.write("};\n\n")

        # Write right channel output
        write_attributes(
            f, attributes, f"{dType} staticRefOutRightCase{testId}[{totalLength}] =\n"
        )
        f.write(
            f"    /* Frame-based output: {frames} frames, {numChannels} channels, {numSamples} samples per channel per frame */\n"
        )
        for frame in range(frames):
            f.write(f"    /* Frame {frame} */\n")
            start_idx = frame * numSamples * numChannels
            end_idx = (frame + 1) * numSamples * numChannels
            f.write(
                "    "
                + ", ".join(f"{x:}" for x in out_right[start_idx:end_idx])
                + ",\n"
            )
        f.write("};\n\n")

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
