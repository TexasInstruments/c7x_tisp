from __future__ import print_function
from subprocess import call
from sys import platform

import sys
import numpy as np
import csv
import os


def write_attributes(f, attributes, stringToWrite):
    f.write("#ifdef WIN32\n")
    f.write(stringToWrite)
    f.write("#else\n")
    f.write(attributes + stringToWrite)
    f.write("#endif\n")
    f.write("{\n")


def write_header_file(input_data, output_data, testCase, headerFileName):
    testId = testCase["ID"]
    fftSize = int(testCase["fftSize"])
    hopSize = int(testCase["hopSize"])
    numChannels = int(testCase["numChannels"])
    numWindows = int(testCase["numWindows"])
    onesided = int(testCase["onesided"])
    numBlocks = int(testCase.get("numBlocks", 1))
    windowType = testCase.get("windowType", "hann")

    attributesIn = '__attribute__((section(".staticData"))) static '
    attributesOut = '__attribute__((section(".staticData"))) static '

    outputDir = "../../../AUDIOLIB/TISP_istft/test_data/"

    if not os.path.exists(outputDir):
        os.makedirs(outputDir)

    with open(outputDir + headerFileName, "w") as f:
        f.write(
            "/* auto-generated test data for TISP_istft, test case %s */\n" % testId
        )
        f.write(
            "/* fftSize=%d hopSize=%d numCh=%d numWindows=%d onesided=%d numBlocks=%d windowType=%s */\n\n"
            % (fftSize, hopSize, numChannels, numWindows, onesided, numBlocks, windowType)
        )

        # input array: complex interleaved floats
        # layout: [numBlocks * numChannels * numWindows * inBins]
        write_attributes(
            f,
            attributesIn,
            "float " + "staticRefInCase" + str(testId) + "[]=\n",
        )
        for idx in range(0, len(input_data)):
            if idx > 0 and idx % 8 == 0:
                f.write("\n")
            f.write("%.10ef, " % input_data[idx])
        f.write("\n};\n\n")

        # output array: real samples
        # layout: [numBlocks * numChannels * numOutputSamples]
        write_attributes(
            f,
            attributesOut,
            "float " + "staticRefOutCase" + str(testId) + "[]=\n",
        )
        for idx in range(0, len(output_data)):
            if idx > 0 and idx % 8 == 0:
                f.write("\n")
            f.write("%.10ef, " % output_data[idx])
        f.write("\n};\n\n")
