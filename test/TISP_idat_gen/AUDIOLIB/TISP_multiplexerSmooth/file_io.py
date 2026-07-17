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


def write_header_file(blocks, in1, in2, out, smoothSamples, testCase, headerFileName):

    testId = testCase["ID"]
    inputs = testCase["inInputs"]
    numSamples = testCase["numSamples"]
    numChannels = testCase["numChannels"]
    isInterleave = testCase["isInterleave"]
    numBlocks = testCase["numBlocks"]
    dType = testCase["dType"]
    attributes = '__attribute__((section(".staticData"))) static '
    outputDir = "../../../AUDIOLIB/TISP_multiplexerSmooth/test_data/"
    os.makedirs(outputDir, exist_ok=True)
    attributesIn0 = '__attribute__((section(".staticData"))) static '
    attributesIn1 = '__attribute__((section(".staticData"))) static '
    attributesIn2 = '__attribute__((section(".staticData"))) static '
    attributesOut = '__attribute__((section(".staticData"))) static '

    with open(outputDir + headerFileName, "w") as f:
        write_attributes(
            f,
            attributesIn0,
            dType + " " + "staticRefIn0Case" + str(testId) + "[]=\n",
        )
        if isInterleave:
            for i in range(numBlocks):
                in0 = blocks[i]
                for dimZ in range(0, inputs):
                    for dimY in range(0, numSamples):
                        for dimX in range(0, numChannels):
                            print(
                                "{:>2}".format(in0[dimZ, dimY, dimX]), file=f, end=", "
                            )
                        print(" ", file=f)
        else:
            for i in range(numBlocks):
                in0 = blocks[i]
                for dimZ in range(0, inputs):
                    for dimY in range(0, numChannels):
                        for dimX in range(0, numSamples):
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
            attributesIn1,
            dType + " " + f"staticRefIn1Case" + str(testId) + "[]=\n",
        )

        for dimX in range(0, smoothSamples):
            print("{:>2}".format(in1[dimX]), file=f, end=", ")
        print(" ", file=f)

        f.write("\n};")

        f.write("\n\n")
        f.close()

    with open(outputDir + headerFileName, "a") as f:
        write_attributes(
            f,
            attributesIn2,
            dType + " " + f"staticRefIn2Case" + str(testId) + "[]=\n",
        )

        for dimX in range(0, smoothSamples):
            print("{:>2}".format(in2[dimX]), file=f, end=", ")
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
        if isInterleave:
            for dimY in range(0, numSamples * numBlocks):
                for dimX in range(0, numChannels):
                    print("{:>2}".format(out[dimY, dimX]), file=f, end=", ")
                print(" ", file=f)
        else:
            for dimY in range(0, numChannels * numBlocks):
                for dimX in range(0, numSamples):
                    print("{:>2}".format(out[dimY, dimX]), file=f, end=", ")
                print(" ", file=f)
        f.write("\n};")
        f.write("\n\n")
        f.close()
