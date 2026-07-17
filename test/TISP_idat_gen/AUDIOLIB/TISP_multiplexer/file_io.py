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


def write_header_file(blocks, out, testCase, headerFileName):
    testId = testCase["ID"]
    samples = testCase["inSamples"]
    channels = testCase["inChannels"]
    dTypeIn0 = testCase["dType"]
    inputs = testCase["inInputs"]
    numBlocks = testCase["numBlocks"]
    totalSize = inputs * samples * channels
    isInterleave = testCase["isInterleave"]

    attributesIn0 = '__attribute__((section(".staticData"))) static '
    attributesOut = '__attribute__((section(".staticData"))) static '

    outputDir = "../../../AUDIOLIB/TISP_multiplexer/test_data/"

    if not os.path.exists(outputDir):
        os.mkdir(outputDir)

    with open(outputDir + headerFileName, "w") as f:
        write_attributes(
            f,
            attributesIn0,
            dTypeIn0 + " " + "staticRefIn0Case" + str(testId) + "[]=\n",
        )
        if isInterleave:
            for i in range(numBlocks):
                in0 = blocks[i]
                for dimZ in range(0, inputs):
                    for dimY in range(0, samples):
                        for dimX in range(0, channels):
                            print(
                                "{:>2}".format(in0[dimZ, dimY, dimX]), file=f, end=", "
                            )
                        print(" ", file=f)
        else:
            for i in range(numBlocks):
                in0 = blocks[i]
                for dimZ in range(0, inputs):
                    for dimY in range(0, channels):
                        for dimX in range(0, samples):
                            print(
                                "{:>2}".format(in0[dimZ, dimY, dimX]), file=f, end=", "
                            )
                        print(" ", file=f)
        f.write("\n};")
        f.write("\n\n")
        f.close()

    with open(outputDir + headerFileName, "a") as f:
        write_attributes(
            f,
            attributesOut,
            dTypeIn0 + " " + "staticRefOutCase" + str(testId) + "[]=\n",
        )

        if isInterleave:
            for dimY in range(0, samples * numBlocks):
                for dimX in range(0, channels):
                    print("{:>2}".format(out[dimY, dimX]), file=f, end=", ")
                print(" ", file=f)
        else:
            for dimY in range(0, channels * numBlocks):
                for dimX in range(0, samples):
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
