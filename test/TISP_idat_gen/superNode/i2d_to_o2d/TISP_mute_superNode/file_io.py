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


def write_header_file(in_data, out_data, gain_data, testCase, headerFileName, frames):
    """Write input, output, and gain data to a C header file.

    Args:
        in_data (list): Flattened input data.
        out_data (list): Flattened output data.
        gain_data (list): Flattened gain data.
        testCase (dict): Test case parameters.
        headerFileName (str): Name of the header file to write.
        frames (int): Number of execution frames.
    """
    testId = testCase["ID"]
    dType = testCase["dType"]
    widthIn = testCase["widthIn"]
    heightIn = testCase["heightIn"]
    isInterleaved = testCase["isInterleaved"]
    totalLength = widthIn * heightIn * frames
    gainLength = widthIn * frames
    attributes = '__attribute__((section(".staticData"))) static '

    outputDir = "../../../../superNode/i2d_to_o2d/TISP_mute_superNode/test_data/"

    if not os.path.exists(outputDir):
        os.makedirs(outputDir)

    with open(os.path.join(outputDir, headerFileName), "w") as f:
        # Define data layout description
        layout_desc = (
            "[ch0_s0, ch1_s0, ch2_s0, ...]"
            if isInterleaved
            else "[ch0_s0, ch0_s1, ch0_s2, ..., ch1_s0, ch1_s1, ...]"
        )

        # Write input data
        write_attributes(
            f, attributes, f"{dType} staticRefInCase{testId}[{totalLength}] =\n"
        )
        f.write(
            f"    /* Frame-based input: {frames} frames, {heightIn} channels, {widthIn} samples per channel per frame, {'interleaved' if isInterleaved else 'non-interleaved'} */\n"
        )
        f.write(f"    /* Data layout: {layout_desc} */\n")
        for frame in range(frames):
            f.write(f"    /* Frame {frame} */\n")
            start_idx = frame * widthIn * heightIn
            end_idx = (frame + 1) * widthIn * heightIn
            f.write(
                "    "
                + ", ".join(f"{x:.6f}f" for x in in_data[start_idx:end_idx])
                + ",\n"
            )
        f.write("};\n\n")

        # Write output data
        write_attributes(
            f, attributes, f"{dType} staticRefOutCase{testId}[{totalLength}] =\n"
        )
        f.write(
            f"    /* Frame-based output: {frames} frames, {heightIn} channels, {widthIn} samples per channel per frame, {'interleaved' if isInterleaved else 'non-interleaved'} */\n"
        )
        f.write(f"    /* Data layout: {layout_desc} */\n")
        for frame in range(frames):
            f.write(f"    /* Frame {frame} */\n")
            start_idx = frame * widthIn * heightIn
            end_idx = (frame + 1) * widthIn * heightIn
            f.write(
                "    "
                + ", ".join(f"{x:.6f}f" for x in out_data[start_idx:end_idx])
                + ",\n"
            )
        f.write("};\n\n")

        # Write gain data
        # write_attributes(
        #     f,
        #     attributes,
        #     f"{dType} staticRefGainCase{testId}[{gainLength}] =\n"
        # )
        # f.write(f"    /* Gain: {frames} frames, {widthIn} samples per frame */\n")
        # for frame in range(frames):
        #     f.write(f"    /* Frame {frame} */\n")
        #     start_idx = frame * widthIn
        #     end_idx = (frame + 1) * widthIn
        #     f.write("    " + ", ".join(f"{x:.6f}f" for x in gain_data[start_idx:end_idx]) + ",\n")
        # f.write("};\n\n")


def write_wav_files(data_input, data_output, data_gain, testCase, num_frames):
    """Write multi-channel WAV files for input and output signals, and a mono WAV file for gain.

    Args:
        data_input (np.ndarray): Input data, shape (num_frames, num_samples * num_channels).
        data_output (np.ndarray): Output data, shape (num_frames, num_samples * num_channels).
        data_gain (np.ndarray): Gain data, shape (num_samples * num_frames,).
        testCase (dict): Test case parameters.
        num_frames (int): Number of execution frames.
    """
    testId = testCase["ID"]
    heightIn = testCase["heightIn"]
    samplingRate = testCase["samplingRate"]
    widthIn = testCase["widthIn"]
    isInterleaved = testCase["isInterleaved"]
    total_samples = widthIn * num_frames

    base_output_dir = (
        "../../../../superNode/i2d_to_o2d/TISP_mute_superNode/test_data/wave_files"
    )
    output_dir = os.path.join(base_output_dir, f"Testcase_{testId}")
    os.makedirs(output_dir, exist_ok=True)

    # Prepare multi-channel arrays
    in_channels = np.zeros((total_samples, heightIn), dtype=np.int16)
    out_channels = np.zeros((total_samples, heightIn), dtype=np.int16)

    # Scale signals to 16-bit PCM range
    max_amplitude = max(np.max(np.abs([data_input, data_output])), 1.0)
    for channel in range(heightIn):
        for frame in range(num_frames):
            sample_start = frame * widthIn
            sample_end = (frame + 1) * widthIn
            if isInterleaved:
                start_idx = channel
                end_idx = heightIn * widthIn
                in_channels[sample_start:sample_end, channel] = (
                    data_input[frame, start_idx:end_idx:heightIn]
                    * 32767.0
                    / max_amplitude
                ).astype(np.int16)
                out_channels[sample_start:sample_end, channel] = (
                    data_output[frame, start_idx:end_idx:heightIn]
                    * 32767.0
                    / max_amplitude
                ).astype(np.int16)
            else:
                start_idx = channel * widthIn
                end_idx = (channel + 1) * widthIn
                in_channels[sample_start:sample_end, channel] = (
                    data_input[frame, start_idx:end_idx] * 32767.0 / max_amplitude
                ).astype(np.int16)
                out_channels[sample_start:sample_end, channel] = (
                    data_output[frame, start_idx:end_idx] * 32767.0 / max_amplitude
                ).astype(np.int16)

    # Scale gain to 16-bit PCM range
    gain_scaled = (data_gain[:total_samples] * 32767.0).astype(np.int16)

    # Write multi-channel WAV files
    wavfile.write(
        os.path.join(output_dir, f"input_case{testId}.wav"), samplingRate, in_channels
    )
    wavfile.write(
        os.path.join(output_dir, f"output_case{testId}.wav"), samplingRate, out_channels
    )
    wavfile.write(
        os.path.join(output_dir, f"gain_case{testId}.wav"), samplingRate, gain_scaled
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
    colors=["blue", "green"],
    amplitude=1.0,
    frequency=2000.0,
    test_type="STATIC",
    is_interleaved=True,
):
    """Plot input/output signals side-by-side or a single plot for gain.

    Args:
        data_left (numpy.ndarray): Input data or gain data.
        data_right (numpy.ndarray): Output data (ignored if signal_type is 'gain').
        num_samples (int): Number of samples per channel per frame.
        num_channels (int): Number of channels.
        num_frames (int): Number of numExecReps.
        test_id (str): Test case ID for the title and subfolder.
        signal_type (str): Type of signal ('input_output' or 'gain').
        channel (int, optional): Channel index for input/output signals.
        colors (list): Colors for plotting.
        amplitude (float): Signal amplitude.
        frequency (float): Signal frequency in Hz.
        test_type (str): Test type ('STATIC' or 'SINE').
        is_interleaved (bool): Whether data is interleaved.
    """
    base_output_dir = (
        "../../../../superNode/i2d_to_o2d/TISP_mute_superNode/test_data/plot_diagrams"
    )
    output_dir = os.path.join(base_output_dir, f"Testcase_{test_id}")
    os.makedirs(output_dir, exist_ok=True)

    total_samples = num_samples * num_frames

    # If the signal is gain, create a single plot
    if signal_type == "gain":
        fig, ax = plt.subplots(1, 1, figsize=(10, 5))
        gain_signal = data_left[:total_samples]

        ax.plot(range(total_samples), gain_signal, label="Gain", color=colors[0])
        ax.set_xlabel("Sample Index")
        ax.set_ylabel("Gain")
        ax.set_title(f"Gain Signal (Test Case {test_id})")
        ax.legend()
        ax.grid(True)
        ax.set_ylim([-0.1, 1.1])

        fig.suptitle(f"Gain Analysis for Test Case {test_id}", fontsize=14)
        filename = f"gain_case{test_id}.png"
        plt.tight_layout(rect=[0, 0.03, 1, 0.95])

    # Otherwise, plot input/output for the specified channel
    else:
        fig, (ax_left, ax_right) = plt.subplots(1, 2, figsize=(12, 5), sharey=True)

        if channel is not None:
            signal_left = np.zeros(total_samples, dtype=np.float32)
            signal_right = np.zeros(total_samples, dtype=np.float32)
            for frame in range(num_frames):
                sample_start = frame * num_samples
                sample_end = (frame + 1) * num_samples
                if is_interleaved:
                    start_idx = channel
                    end_idx = num_channels * num_samples
                    signal_left[sample_start:sample_end] = data_left[
                        frame, start_idx:end_idx:num_channels
                    ]
                    signal_right[sample_start:sample_end] = data_right[
                        frame, start_idx:end_idx:num_channels
                    ]
                else:
                    start_idx = channel * num_samples
                    end_idx = (channel + 1) * num_samples
                    signal_left[sample_start:sample_end] = data_left[
                        frame, start_idx:end_idx
                    ]
                    signal_right[sample_start:sample_end] = data_right[
                        frame, start_idx:end_idx
                    ]

            label_left = f"Input Channel {channel}"
            label_right = f"Output Channel {channel}"
            filename = f"input_output_case{test_id}_ch{channel}.png"

            ax_left.plot(
                range(total_samples), signal_left, label=label_left, color=colors[0]
            )
            ax_left.set_xlabel("Sample Index")
            ax_left.set_ylabel("Amplitude")
            ax_left.set_title(f"{label_left}")
            ax_left.legend()
            ax_left.grid(True)

            ax_right.plot(
                range(total_samples), signal_right, label=label_right, color=colors[1]
            )
            ax_right.set_xlabel("Sample Index")
            ax_right.set_title(f"{label_right}")
            ax_right.legend()
            ax_right.grid(True)

            fig.suptitle(f"Input/Output Signals for Test Case {test_id}", fontsize=14)
            plt.tight_layout(rect=[0, 0.03, 1, 0.95])
        else:
            print(
                f"Warning: Plotting for signal_type '{signal_type}' requires a channel index."
            )
            plt.close()
            return

    plt.savefig(os.path.join(output_dir, filename), bbox_inches="tight")
    plt.close()
