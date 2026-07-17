from __future__ import print_function
from subprocess import call
from sys import platform

import sys
import numpy as np
import csv
import os

import matplotlib.pyplot as plt


def write_attributes(f, attributes, stringToWrite):
    """Helper function to write platform-specific C attributes."""
    f.write("#ifdef WIN32\n")
    f.write(stringToWrite)
    f.write("#else\n")
    f.write(attributes + stringToWrite)
    f.write("#endif\n")
    f.write("{\n")


def plot_Signals(out, totalSamples, testId):
    """Plots the generated sinusoid signal and saves it to a file.

    Args:
        out (numpy.ndarray): The array of output samples.
        totalSamples (int): The total number of samples in 'out' (numSamples * numExecReps).
        testId (str): The test case ID for naming.
    """
    base_output_dir = "../../../AUDIOLIB/TISP_sinusoidGenerator/test_data/plot_diagrams"
    output_dir = os.path.join(base_output_dir, f"Testcase_{testId}")
    os.makedirs(output_dir, exist_ok=True)

    filename = f"Testcase_{testId}.png"

    plt.figure(figsize=(10, 4))  # Create a new figure

    # Plot the entire signal using totalSamples
    plt.plot(range(totalSamples), out, label="Output", color="red")
    plt.title(f"sinusoidGenerator (Test Case {testId}) - {totalSamples} samples")
    plt.xlabel("Sample Index")
    plt.ylabel("Amplitude")
    plt.grid(True)
    plt.legend()

    plt.tight_layout()

    # Save before showing (to avoid empty file issues in some backends)
    plt.savefig(os.path.join(output_dir, filename), bbox_inches="tight")
    plt.close()


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

    outputDir = "../../../AUDIOLIB/TISP_sinusoidGenerator/test_data/"

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

            # Write samples for this frame, joining with commas
            samples_str = ", ".join(f"{x:}" for x in out[start_idx:end_idx])
            f.write("    " + samples_str + ",\n")

        f.write("};")
        f.write("\n\n")
        # No f.close() needed when using 'with open()'

    if platform == "linux" or platform == "linux2":
        try:
            call(
                [
                    "indent",
                    "-nut",
                    "-i3",
                    # "-c55",
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
