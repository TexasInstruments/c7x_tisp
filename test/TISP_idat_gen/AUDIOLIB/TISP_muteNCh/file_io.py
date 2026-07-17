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
    """Write input, output, gain, and isMuted data to a C header file.

    Args:
        in_data (list): Flattened input data.
        out_data (list): Flattened output data.
        gain_data (list): Flattened gain data (numChannels * totalSamplesPerChannel).
        testCase (dict): Test case parameters.
        headerFileName (str): Name of the header file to write.
        frames (int): Number of execution frames.
    """
    testId = testCase["ID"]
    numSamples = testCase["numSamples"]
    numChannels = testCase["numChannels"]
    dType = testCase["dType"]
    isInterleaved = testCase["isInterleaved"]
    totalLength = numSamples * numChannels * frames
    totalSamplesPerChannel = numSamples * frames
    gainLength = numChannels * totalSamplesPerChannel
    attributes = '__attribute__((section(".staticData"))) static '
    outputDir = "../../../AUDIOLIB/TISP_muteNCh/test_data/"

    if not os.path.exists(outputDir):
        os.makedirs(outputDir)

    with open(os.path.join(outputDir, headerFileName), "w") as f:
        # Write isMuted data (new section)
        isMuted_data = [int(m) for m in testCase["isMuted"]]  # Convert to ints
        write_attributes(
            f, attributes, f"uint32_t staticRefIsMutedCase{testId}[{numChannels}] =\n"
        )
        f.write(f"    /* isMuted array: one value per channel, 1=mute, 0=unmute */\n")
        f.write("    " + ", ".join(str(x) for x in isMuted_data) + ",\n")
        f.write("};\n\n")

        # Define data layout description for input/output
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
            f"    /* Frame-based input: {frames} frames, {numChannels} channels, {numSamples} samples per channel per frame, {'interleaved' if isInterleaved else 'non-interleaved'} */\n"
        )
        f.write(f"    /* Data layout: {layout_desc} */\n")
        frame_start_base = 0
        for frame in range(frames):
            f.write(f"    /* Frame {frame} */\n")
            frame_start = frame_start_base
            frame_end = frame_start + numSamples * numChannels
            if not isInterleaved:
                for ch in range(numChannels):
                    ch_start = frame_start + ch * numSamples
                    ch_end = ch_start + numSamples
                    f.write(f"        /* Start of Channel {ch} */\n")
                    f.write(
                        "        "
                        + ", ".join(f"{x:.6f}f" for x in in_data[ch_start:ch_end])
                        + ",\n"
                    )
                    f.write(f"        /* End of Channel {ch} */\n")
            else:
                f.write(
                    "    "
                    + ", ".join(f"{x:.6f}f" for x in in_data[frame_start:frame_end])
                    + ",\n"
                )
                f.write(f"    /* Interleaved channels within frame */\n")
            frame_start_base = frame_end
        f.write("};\n\n")

        # Write output data
        write_attributes(
            f, attributes, f"{dType} staticRefOutCase{testId}[{totalLength}] =\n"
        )
        f.write(
            f"    /* Frame-based output: {frames} frames, {numChannels} channels, {numSamples} samples per channel per frame, {'interleaved' if isInterleaved else 'non-interleaved'} */\n"
        )
        f.write(f"    /* Data layout: {layout_desc} */\n")
        frame_start_base = 0
        for frame in range(frames):
            f.write(f"    /* Frame {frame} */\n")
            frame_start = frame_start_base
            frame_end = frame_start + numSamples * numChannels
            if not isInterleaved:
                for ch in range(numChannels):
                    ch_start = frame_start + ch * numSamples
                    ch_end = ch_start + numSamples
                    f.write(f"        /* Start of Channel {ch} */\n")
                    f.write(
                        "        "
                        + ", ".join(f"{x:.6f}f" for x in out_data[ch_start:ch_end])
                        + ",\n"
                    )
                    f.write(f"        /* End of Channel {ch} */\n")
            else:
                f.write(
                    "    "
                    + ", ".join(f"{x:.6f}f" for x in out_data[frame_start:frame_end])
                    + ",\n"
                )
                f.write(f"    /* Interleaved channels within frame */\n")
            frame_start_base = frame_end
        f.write("};\n\n")

        # Write gain data
        # write_attributes(
        #     f,
        #     attributes,
        #     f"{dType} staticRefGainCase{testId}[{gainLength}] =\n"
        # )
        # f.write(f"    /* Gain: {numChannels} channels, {frames} frames, {numSamples} samples per channel per frame, non-interleaved */\n")
        # f.write(f"    /* Data layout: [ch0_frame0_s0, ch0_frame0_s1, ..., ch0_frame1_s0, ..., ch1_frame0_s0, ...] */\n")
        # for ch in range(numChannels):
        #     f.write(f"    /* Channel {ch} */\n")
        #     for frame in range(frames):
        #         f.write(f"        /* Frame {frame} */\n")
        #         start_idx = ch * totalSamplesPerChannel + frame * numSamples
        #         end_idx = start_idx + numSamples
        #         f.write("    " + ", ".join(f"{x:.6f}f" for x in gain_data[start_idx:end_idx]) + ",\n")
        # f.write("};\n\n")


def write_wav_files(data_input, data_output, data_gain, testCase, num_frames):
    """Write multi-channel WAV files for input and output signals, and a multi-channel WAV file for gains.

    Args:
        data_input (np.ndarray): Input data, shape (num_frames, num_samples * num_channels).
        data_output (np.ndarray): Output data, shape (num_frames, num_samples * num_channels).
        data_gain (np.ndarray): Gain data, shape (num_channels, num_samples * num_frames).
        testCase (dict): Test case parameters.
        num_frames (int): Number of execution frames.
    """
    testId = testCase["ID"]
    numChannels = testCase["numChannels"]
    samplingRate = testCase["samplingRate"]
    numSamples = testCase["numSamples"]
    isInterleaved = testCase["isInterleaved"]
    total_samples = numSamples * num_frames

    base_output_dir = "../../../AUDIOLIB/TISP_muteNCh/test_data/wave_files"
    output_dir = os.path.join(base_output_dir, f"Testcase_{testId}")
    os.makedirs(output_dir, exist_ok=True)

    # Prepare multi-channel arrays
    in_channels = np.zeros((total_samples, numChannels), dtype=np.int16)
    out_channels = np.zeros((total_samples, numChannels), dtype=np.int16)
    gain_channels = np.zeros((total_samples, numChannels), dtype=np.int16)

    # Scale signals to 16-bit PCM range
    max_amplitude = max(np.max(np.abs([data_input, data_output])), 1.0)
    for channel in range(numChannels):
        for frame in range(num_frames):
            sample_start = frame * numSamples
            sample_end = (frame + 1) * numSamples
            if isInterleaved:
                start_idx = channel
                end_idx = numChannels * numSamples
                in_channels[sample_start:sample_end, channel] = (
                    data_input[frame, start_idx:end_idx:numChannels]
                    * 32767.0
                    / max_amplitude
                ).astype(np.int16)
                out_channels[sample_start:sample_end, channel] = (
                    data_output[frame, start_idx:end_idx:numChannels]
                    * 32767.0
                    / max_amplitude
                ).astype(np.int16)
            else:
                start_idx = channel * numSamples
                end_idx = (channel + 1) * numSamples
                in_channels[sample_start:sample_end, channel] = (
                    data_input[frame, start_idx:end_idx] * 32767.0 / max_amplitude
                ).astype(np.int16)
                out_channels[sample_start:sample_end, channel] = (
                    data_output[frame, start_idx:end_idx] * 32767.0 / max_amplitude
                ).astype(np.int16)
        # Scale gain for this channel
        gain_channels[:, channel] = (
            data_gain[channel, :total_samples] * 32767.0
        ).astype(np.int16)

    # Write multi-channel WAV files
    wavfile.write(
        os.path.join(output_dir, f"input_case{testId}.wav"), samplingRate, in_channels
    )
    wavfile.write(
        os.path.join(output_dir, f"output_case{testId}.wav"), samplingRate, out_channels
    )
    wavfile.write(
        os.path.join(output_dir, f"gain_case{testId}.wav"), samplingRate, gain_channels
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
    """Plot input/output signals side-by-side or gain for a single channel.

    Args:
        data_left (numpy.ndarray): Input data or gain data (for gain: shape (num_channels, total_samples)).
        data_right (numpy.ndarray): Output data (ignored if signal_type is 'gain').
        num_samples (int): Number of samples per channel per frame.
        num_channels (int): Number of channels.
        num_frames (int): Number of numExecReps.
        test_id (str): Test case ID for the title and subfolder.
        signal_type (str): Type of signal ('input_output' or 'gain').
        channel (int): Channel index (required for both types now).
        colors (list): Colors for plotting.
        amplitude (float): Signal amplitude.
        frequency (float): Signal frequency in Hz.
        test_type (str): Test type ('STATIC' or 'SINE').
        is_interleaved (bool): Whether data is interleaved.
    """
    if channel is None:
        print(f"Warning: channel must be specified for plotting.")
        return

    base_output_dir = "../../../AUDIOLIB/TISP_muteNCh/test_data/plot_diagrams"
    output_dir = os.path.join(base_output_dir, f"Testcase_{test_id}")
    os.makedirs(output_dir, exist_ok=True)

    total_samples = num_samples * num_frames

    # If the signal is gain, create a single plot for the specified channel
    if signal_type == "gain":
        fig, ax = plt.subplots(1, 1, figsize=(10, 5))
        gain_signal = data_left[channel, :total_samples]
        ax.plot(
            range(total_samples),
            gain_signal,
            label=f"Gain Ch {channel}",
            color=colors[0],
        )
        ax.set_xlabel("Sample Index")
        ax.set_ylabel("Gain")
        ax.set_title(f"Gain Signal for Channel {channel} (Test Case {test_id})")
        ax.legend()
        ax.grid(True)
        ax.set_ylim([-0.1, 1.1])

        fig.suptitle(
            f"Gain Evolution for Channel {channel} in Test Case {test_id}", fontsize=14
        )
        filename = f"gain_case{test_id}_ch{channel}.png"
        plt.tight_layout()

    # Otherwise, plot input/output for the specified channel side-by-side
    else:
        fig, (ax_left, ax_right) = plt.subplots(1, 2, figsize=(12, 5), sharey=True)

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

        fig.suptitle(
            f"Input/Output Signals for Channel {channel} in Test Case {test_id}",
            fontsize=14,
        )
        plt.tight_layout()

    plt.savefig(os.path.join(output_dir, filename), bbox_inches="tight")
    plt.close()
