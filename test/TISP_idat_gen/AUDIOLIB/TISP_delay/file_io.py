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


def write_header_file(
    in0, output_buffer, updated_state_buffer, testCase, headerFileName, delaySize
):
    testId = testCase["ID"]
    numChannels = testCase["numChannels"]
    numSamples = testCase["numSamples"]
    dType = testCase["dType"]
    interleave = testCase["interleave"]
    numBlocks = testCase["numBlocks"]

    attributesIn0 = '__attribute__((section(".staticData"))) static '
    attributesOut0 = '__attribute__((section(".staticData"))) static '
    attributesOut1 = '__attribute__((section(".staticData"))) static '

    outputDir = "../../../AUDIOLIB/TISP_delay/test_data/"

    if not os.path.exists(outputDir):
        os.mkdir(outputDir)

    with open(outputDir + headerFileName, "w") as f:
        write_attributes(
            f,
            attributesIn0,
            dType + " " + "staticRefInCase" + str(testId) + "[]=\n",
        )

        if interleave:
            for frame in range(0, numBlocks):
                print("\t// Frame: ", frame, file=f)
                for dimY in range(frame * numSamples, (frame + 1) * numSamples):
                    for dimX in range(0, numChannels):
                        print("{:>2}".format(in0[dimY, dimX]), file=f, end=", ")
                    print(" ", file=f)
        else:
            for frame in range(0, numBlocks):
                print("\t// Frame: ", frame, file=f)
                for dimY in range(0, numChannels):
                    for dimX in range(frame * numSamples, (frame + 1) * numSamples):
                        print("{:>2}".format(in0[dimY, dimX]), file=f, end=", ")
                    print(" ", file=f)

        f.write("\n};")
        f.write("\n\n")
        f.close()

    with open(outputDir + headerFileName, "a") as f:
        write_attributes(
            f,
            attributesOut0,
            dType + " " + "staticRefOutCase" + str(testId) + "[]=\n",
        )

        if interleave:
            for frame in range(0, numBlocks):
                print("\t// Frame: ", frame, file=f)
                for dimY in range(frame * numSamples, (frame + 1) * numSamples):
                    for dimX in range(0, numChannels):
                        print(
                            "{:>2}".format(output_buffer[dimY, dimX]), file=f, end=", "
                        )
                    print(" ", file=f)
        else:
            for frame in range(0, numBlocks):
                print("\t// Frame: ", frame, file=f)
                for dimY in range(0, numChannels):
                    for dimX in range(frame * numSamples, (frame + 1) * numSamples):
                        print(
                            "{:>2}".format(output_buffer[dimY, dimX]), file=f, end=", "
                        )
                    print(" ", file=f)

        f.write("\n};")
        f.write("\n\n")
        f.close()

    with open(outputDir + headerFileName, "a") as f:
        write_attributes(
            f,
            attributesOut1,
            dType + " " + "staticUpdatedDelayCase" + str(testId) + "[]=\n",
        )
        for dimY in range(0, numChannels):
            for dimX in range(0, delaySize):
                print(
                    "{:>2}".format(updated_state_buffer[dimY, dimX]), file=f, end=", "
                )
            print(" ", file=f)
        f.write("\n};")
        f.write("\n\n")
        f.close()

    """if platform == "linux" or platform == "linux2":
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
