from __future__ import print_function
from subprocess import call
from sys import platform

import os
import numpy as np
import matplotlib.pyplot as plt
from scipy.io import wavfile


def write_attributes(f, attributes, stringToWrite):
    """Write data arrays with platform-specific attributes."""
    f.write("#ifdef WIN32\n")
    f.write(stringToWrite)
    f.write("#else\n")
    f.write(attributes + stringToWrite)
    f.write("#endif\n")
    f.write("{\n")


def write_header_file(
    in_signal,
    desired_signal,
    out_signal,
    coefficients,
    testCase,
    headerFileName,
    frames,
):
    """Generate header file with NLMS test data."""
    testId = testCase["ID"]
    numChannels = int(testCase["numChannels"])
    numSamples = int(testCase["numSamples"])
    filterLength = int(testCase["filterLength"])
    dType = testCase.get("dType", "float")
    isInterleaved = int(testCase.get("isInterleaved", 0))

    if isInterleaved:
        totalSamples = numSamples * numChannels * frames
        samplesPerFrame = numSamples * numChannels
    else:
        samplesPerFrame = numSamples * numChannels
        totalSamples = samplesPerFrame * frames

    attributesIn = '__attribute__((section(".staticData"))) static '
    attributesDesired = '__attribute__((section(".staticData"))) static '
    attributesOut = '__attribute__((section(".staticData"))) static '
    attributesCoeff = '__attribute__((section(".staticData"))) static '

    outputDir = "../../../../superNode/i2d_i2d_to_o2d/TISP_nlms_superNode/test_data/"
    os.makedirs(outputDir, exist_ok=True)

    fullpath = os.path.join(outputDir, headerFileName)

    fmt_str = "{:>15.8e}"
    fmt_out = "{: .6f}"

    # Write input signal
    with open(fullpath, "w") as f:
        write_attributes(
            f,
            attributesIn,
            dType + " " + "staticRefInCase" + str(testId) + "[]=\n",
        )

        if isInterleaved:
            for i, val in enumerate(in_signal):
                f.write(fmt_str.format(val))
                f.write(",")
                if (i + 1) % numChannels == 0:
                    f.write("\n")
        else:
            for i, val in enumerate(in_signal):
                f.write(fmt_str.format(val))
                f.write(",")
                if (i + 1) % samplesPerFrame == 0:
                    f.write("\n")
        f.write("\n};\n\n")

    # Write desired signal
    with open(fullpath, "a") as f:
        write_attributes(
            f,
            attributesDesired,
            dType + " " + "staticRefDesiredCase" + str(testId) + "[]=\n",
        )

        if isInterleaved:
            for i, val in enumerate(desired_signal):
                f.write(fmt_str.format(val))
                f.write(",")
                if (i + 1) % numChannels == 0:
                    f.write("\n")
        else:
            for i, val in enumerate(desired_signal):
                f.write(fmt_str.format(val))
                f.write(",")
                if (i + 1) % samplesPerFrame == 0:
                    f.write("\n")
        f.write("\n};\n\n")

    # Write output signal
    with open(fullpath, "a") as f:
        write_attributes(
            f,
            attributesOut,
            dType + " " + "staticRefOutCase" + str(testId) + "[]=\n",
        )
        if isInterleaved:
            for i, val in enumerate(out_signal):
                f.write(fmt_out.format(val))
                f.write(",")
                if (i + 1) % numChannels == 0:
                    f.write("\n")
        else:
            for i, val in enumerate(out_signal):
                f.write(fmt_out.format(val))
                f.write(",")
                if (i + 1) % samplesPerFrame == 0:
                    f.write("\n")
        f.write("\n};\n\n")

    # Write filter coefficients
    with open(fullpath, "a") as f:
        write_attributes(
            f,
            attributesCoeff,
            dType + " " + "staticRefCoeffCase" + str(testId) + "[]=\n",
        )

        for i, val in enumerate(coefficients):
            f.write(fmt_str.format(val))  # Use dynamic format
            f.write(",")
            if (i + 1) % filterLength == 0:
                f.write("\n")
        f.write("\n};\n\n")

    # Format the file
    # if platform == "linux" or platform == "linux2":
    #    try:
    #        call(["indent", "-nut", "-i3", "-l200", fullpath])
    #        call(["rm", fullpath + "~"])
    #    except Exception:
    #        pass


def plot_signal_pair(
    signal1,
    signal2,
    numSamples,
    numChannels,
    numFrames,
    testId,
    plot_type,
    channel=0,
    colors=None,
    amplitude=1.0,
    frequency=1000.0,
    test_type="STATIC",
    is_interleaved=0,
):
    """Plot a pair of signals for visualization and save PNG(s)."""
    if colors is None:
        colors = ["C0", "C1"]

    outputDir = "../../../../superNode/i2d_i2d_to_o2d/TISP_nlms_superNode/plots/"
    os.makedirs(outputDir, exist_ok=True)

    # Extract channel data based on layout
    if is_interleaved:
        channel_signal1 = np.array(signal1).flatten()[channel::numChannels]
        channel_signal2 = np.array(signal2).flatten()[channel::numChannels]
    else:
        startIdx = channel * numSamples
        endIdx = (channel + 1) * numSamples
        channel_signal1 = np.array(signal1)[:, startIdx:endIdx].flatten()
        channel_signal2 = np.array(signal2)[:, startIdx:endIdx].flatten()

    totalSamples = len(channel_signal1)
    time_axis = np.arange(totalSamples)

    plotFileName = f"{outputDir}test_{testId}_{plot_type}_ch{channel}.png"

    if plot_type == "output_desired":
        fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 8))

        ax1.plot(time_axis, channel_signal1, label="Output")
        ax1.plot(time_axis, channel_signal2, label="Desired")
        ax1.set_xlabel("Sample Index")
        ax1.set_ylabel("Amplitude")
        layout_str = "INTERLEAVED" if is_interleaved else "NON-INTERLEAVED"
        ax1.set_title(
            f"Test {testId} - {plot_type} - Channel {channel} ({test_type}, {layout_str})"
        )
        ax1.legend()
        ax1.grid(True, alpha=0.3)

        error = channel_signal1 - channel_signal2
        ax2.plot(time_axis, error, label="Error (Output - Desired)")
        ax2.axhline(y=0, linestyle="--", linewidth=0.6)
        ax2.set_xlabel("Sample Index")
        ax2.set_ylabel("Error Amplitude")
        ax2.set_title(f"Convergence Error - Test {testId} - Channel {channel}")
        ax2.legend()
        ax2.grid(True, alpha=0.3)

        plt.tight_layout()
        plt.savefig(plotFileName, dpi=150, bbox_inches="tight")
        plt.close(fig)
    else:
        plt.figure(figsize=(12, 4))
        plt.plot(time_axis, channel_signal1, label="Signal 1")
        plt.plot(time_axis, channel_signal2, label="Signal 2")
        plt.xlabel("Sample Index")
        plt.ylabel("Amplitude")
        layout_str = "INTERLEAVED" if is_interleaved else "NON-INTERLEAVED"
        plt.title(
            f"Test {testId} - {plot_type} - Channel {channel} ({test_type}, {layout_str})"
        )
        plt.legend()
        plt.grid(True, alpha=0.3)
        plt.savefig(plotFileName, dpi=150, bbox_inches="tight")
        plt.close()


def write_wav_files(
    pInSignal,
    pDesiredSignal,
    pOutSignal,
    pErrorSignal,
    pResidualSignal,
    pNoiseSignal,
    testCase,
    numFrames,
):
    """Write WAV files for audio signals."""
    testId = testCase["ID"]
    numChannels = int(testCase["numChannels"])
    numSamples = int(testCase["numSamples"])
    samplingRate = int(testCase["samplingRate"])
    isInterleaved = int(testCase.get("isInterleaved", 0))

    outputDir = "../../../../superNode/i2d_i2d_to_o2d/TISP_nlms_superNode/wav_files/"
    os.makedirs(outputDir, exist_ok=True)

    def normalize_to_int16(signal):
        signal = np.asarray(signal, dtype=np.float64)
        max_val = np.max(np.abs(signal)) if signal.size else 0.0
        if max_val > 0:
            normalized = signal / max_val * 32767 * 0.95
        else:
            normalized = signal
        return normalized.astype(np.int16)

    for ch in range(numChannels):
        if isInterleaved:
            in_ch = np.array(pInSignal).flatten()[ch::numChannels]
            desired_ch = np.array(pDesiredSignal).flatten()[ch::numChannels]
            out_ch = np.array(pOutSignal).flatten()[ch::numChannels]
        else:
            startIdx = ch * numSamples
            endIdx = (ch + 1) * numSamples
            in_ch = np.array(pInSignal)[:, startIdx:endIdx].flatten()
            desired_ch = np.array(pDesiredSignal)[:, startIdx:endIdx].flatten()
            out_ch = np.array(pOutSignal)[:, startIdx:endIdx].flatten()

        wavfile.write(
            os.path.join(outputDir, f"test_{testId}_input_ch{ch}.wav"),
            samplingRate,
            normalize_to_int16(in_ch),
        )
        wavfile.write(
            os.path.join(outputDir, f"test_{testId}_desired_ch{ch}.wav"),
            samplingRate,
            normalize_to_int16(desired_ch),
        )
        wavfile.write(
            os.path.join(outputDir, f"test_{testId}_output_ch{ch}.wav"),
            samplingRate,
            normalize_to_int16(out_ch),
        )

    layout_str = "INTERLEAVED" if isInterleaved else "NON-INTERLEAVED"
    print(f"WAV files written for test {testId} ({layout_str})")
