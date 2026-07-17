from __future__ import print_function
from subprocess import call
from sys import platform

import sys
import numpy as np
import csv
import os

import matplotlib.pyplot as plt


def write_attributes(f, attributes, stringToWrite):
    f.write("#ifdef WIN32\n")
    f.write(stringToWrite)
    f.write("#else\n")
    f.write(attributes + stringToWrite)
    f.write("#endif\n")
    f.write("{\n")


def plot_Signals(in0, out, numSamples, numChannels, testId):
    base_output_dir = "../../../../superNode/i2d_to_o2d/TISP_softClip_superNode/test_data/plot_diagrams"
    output_dir = os.path.join(base_output_dir, f"Testcase_{testId}")
    os.makedirs(output_dir, exist_ok=True)

    filename = f"Testcase_{testId}.png"

    plot_channels = min(numChannels, 4)
    fig, axes = plt.subplots(plot_channels, 1, figsize=(10, 8), sharex=True)

    # If only 1 channel, wrap in a list for consistency
    if numChannels == 1:
        axes = [axes]
    else:
        axes = axes

    for ch in range(plot_channels):
        if ch < 4:
            axes[ch].plot(in0[ch, :], label="Input", color="blue")
            axes[ch].plot(out[ch, :], label="Output", color="red", linestyle="--")
            axes[ch].set_title(f"Channel {ch+1}")
            axes[ch].set_ylabel("Amplitude")
            axes[ch].grid(True)
            axes[ch].legend()

    plt.xlabel("Sample Index")
    plt.tight_layout()

    # Save before showing (to avoid empty file issues in some backends)
    plt.savefig(os.path.join(output_dir, filename), bbox_inches="tight")
    # plt.show()
    plt.close()


def write_header_file(in0, out, testCase, headerFileName):
    testId = testCase["ID"]
    dType = testCase["dType"]
    widthIn = testCase["widthIn"]
    heightIn = testCase["heightIn"]

    attributesIn0 = '__attribute__((section(".staticData"))) static '
    attributesOut = '__attribute__((section(".staticData"))) static '

    outputDir = "../../../../superNode/i2d_to_o2d/TISP_softClip_superNode/test_data/"

    if not os.path.exists(outputDir):
        os.mkdir(outputDir)

    with open(outputDir + headerFileName, "w") as f:
        write_attributes(
            f,
            attributesIn0,
            dType + " " + "staticRefIn0Case" + str(testId) + "[]=\n",
        )
        for dimY in range(0, heightIn):
            for dimX in range(0, widthIn):
                print("{:>2}".format(in0[dimY, dimX]), file=f, end=", ")
            print(" ", file=f)

        f.write("\n};")
        f.write("\n\n")
        f.close()

    with open(outputDir + headerFileName, "a") as f:
        write_attributes(
            f,
            attributesOut,
            dType + " " + "staticRefOutCase" + str(testId) + "[]=\n",
        )
        for dimY in range(0, heightIn):
            for dimX in range(0, widthIn):
                print("{:>2}".format(out[dimY, dimX]), file=f, end=", ")
            print(" ", file=f)
        f.write("\n};")
        f.write("\n\n")
        f.close()


"""
    if platform == "linux" or platform == "linux2":
        call(
            [
                "indent",
                "-nut",
                "-i3",
                # "-c55",
                "-l100",
                outputDir + headerFileName,
            ]
        )
        call(["rm", outputDir + headerFileName + "~"])  # remove backup file"""
