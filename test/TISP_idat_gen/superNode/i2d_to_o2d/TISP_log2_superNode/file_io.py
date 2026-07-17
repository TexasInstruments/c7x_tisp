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


def write_header_file(in0, out, testCase, headerFileName):
    testId = testCase["ID"]
    length = testCase["length"]
    dTypeIn = testCase["dType"]

    attributesIn = '__attribute__((section(".staticData"))) static '
    attributesOut = '__attribute__((section(".staticData"))) static '

    outputDir = "../../../../superNode/i2d_to_o2d/TISP_log2_superNode/test_data/"

    if not os.path.exists(outputDir):
        os.mkdir(outputDir)

    with open(outputDir + headerFileName, "w") as f:
        write_attributes(
            f,
            attributesIn,
            dTypeIn + " " + "staticRefInCase" + str(testId) + "[]=\n",
        )
        for dimX in range(0, length):
            print("{:>2}".format(in0[dimX]), file=f, end=", ")
        f.write("\n};")
        f.write("\n\n")
        f.close()

    with open(outputDir + headerFileName, "a") as f:
        write_attributes(
            f,
            attributesOut,
            dTypeIn + " " + "staticRefOutCase" + str(testId) + "[]=\n",
        )
        for dimX in range(0, length):
            print("{:>2}".format(out[dimX]), file=f, end=", ")
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
