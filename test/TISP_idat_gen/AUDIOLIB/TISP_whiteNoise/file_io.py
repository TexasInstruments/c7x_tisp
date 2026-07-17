from __future__ import print_function
from subprocess import call
from sys import platform

import os
import numpy as np
import matplotlib.pyplot as plt
from scipy.io import wavfile


# ===============================================================
# Helper: Platform-specific attribute writer
# ===============================================================
def write_attributes(f, attributes, stringToWrite):
    """Helper function to write platform-specific C attributes."""
    f.write("#ifdef WIN32\n")
    f.write(stringToWrite)
    f.write("#else\n")
    f.write(attributes + stringToWrite)
    f.write("#endif\n")
    f.write("{\n")


# ===============================================================
# Plot Signal (frame-aware)
# ===============================================================
def plot_Signals(out, totalSamples, testId):
    """Plots the generated white noise signal and saves it to a file.

    Args:
        out (numpy.ndarray): The array of output samples.
        totalSamples (int): The total number of samples in 'out' (numSamples * numExecReps).
        testId (str): The test case ID for naming.
    """
    base_output_dir = "../../../AUDIOLIB/TISP_whiteNoise/test_data/plot_diagrams"
    output_dir = os.path.join(base_output_dir, f"Testcase_{testId}")
    os.makedirs(output_dir, exist_ok=True)

    filename = f"Testcase_{testId}.png"

    plt.figure(figsize=(10, 4))
    plt.plot(range(totalSamples), out, label="White Noise Output", color="blue")
    plt.title(f"whiteNoiseGenerator (Test Case {testId}) - {totalSamples} samples")
    plt.xlabel("Sample Index")
    plt.ylabel("Amplitude")
    plt.grid(True)
    plt.legend()
    plt.tight_layout()

    plt.savefig(os.path.join(output_dir, filename), bbox_inches="tight")
    plt.close()


# ===============================================================
# Write WAV File
# ===============================================================
def write_wave_file(out, testCase, waveFileName):
    """Writes the generated signal data to a WAV audio file using scipy.

    Args:
        out (numpy.ndarray): The array of output samples (float32 or float64).
        testCase (dict): Test case information containing samplingRate, testId, etc.
        waveFileName (str): The name of the output WAV file.
    """
    testId = testCase["testId"]
    samplingRate = testCase["samplingRate"]
    numSamples = testCase["numSamples"]
    numBlocks = testCase.get("numBlocks", 1)
    totalSamples = len(out)

    outputDir = "../../../AUDIOLIB/TISP_whiteNoise/test_data/audio_files/"
    os.makedirs(outputDir, exist_ok=True)

    wavFilePath = os.path.join(outputDir, waveFileName)

    # Normalize to prevent clipping
    max_val = np.max(np.abs(out))
    normalized_out = out / max_val if max_val > 0 else out

    # Convert to 16-bit PCM
    audio_data = np.int16(normalized_out * 32767)

    try:
        wavfile.write(wavFilePath, samplingRate, audio_data)

        print(f"✓ WAV file created: {wavFilePath}")
        print(f"  - Sample rate: {samplingRate} Hz")
        print(f"  - Duration: {totalSamples / samplingRate:.2f} seconds")
        print(
            f"  - Samples: {totalSamples} ({numBlocks} frames × {numSamples} samples/frame)"
        )
        print(f"  - File size: {os.path.getsize(wavFilePath)/1024:.2f} KB")

    except Exception as e:
        print(f"✗ Error writing WAV file: {e}")


# ===============================================================
# Write C Header File (frame-aware, consistent format)
# ===============================================================
def write_header_file(out, testCase, headerFileName):
    """Writes the generated signal data to a C header file.

    Args:
        out (numpy.ndarray): The array of output samples (totalSamples long).
        testCase (dict): The dictionary of test case parameters.
        headerFileName (str): The name for the output .h file.
    """
    testId = testCase["ID"]
    numSamples = testCase["numSamples"]  # Samples *per frame*
    numBlocks = testCase["numBlocks"]  # Number of frames
    totalSamples = numSamples * numBlocks  # Total samples in the 'out' array

    dType = testCase["dType"]

    attributesOut = '__attribute__((section(".staticData"))) static '

    outputDir = "../../../AUDIOLIB/TISP_whiteNoise/test_data/"

    if not os.path.exists(outputDir):
        os.mkdir(outputDir)

    with open(outputDir + headerFileName, "w") as f:
        write_attributes(
            f,
            attributesOut,
            # Explicitly define the array size using totalSamples
            dType + " " + "staticRefOutCase" + str(testId) + f"[{totalSamples}]=\n",
        )

        f.write(
            f"    /* Frame-based output: {numBlocks} frames, {numSamples} samples per frame */\n"
        )

        # Loop frame by frame to write data with comments
        for frame in range(numBlocks):
            f.write(f"    /* Frame {frame} */\n")
            start_idx = frame * numSamples
            end_idx = (frame + 1) * numSamples

            frame_samples = ", ".join(f"{x:}" for x in out[start_idx:end_idx])
            f.write("    " + frame_samples + ",\n")

        f.write("};\n\n")

    # Optional Linux formatting
    if platform == "linux" or platform == "linux2":
        try:
            call(
                [
                    "indent",
                    "-nut",
                    "-i3",
                    "-l100",
                    os.path.join(outputDir, headerFileName),
                ]
            )
            call(
                ["rm", os.path.join(outputDir, headerFileName + "~")]
            )  # remove backup file
        except OSError as e:
            print(
                f"Warning: Formatting command (indent/rm) failed. Is it installed? Error: {e}"
            )
