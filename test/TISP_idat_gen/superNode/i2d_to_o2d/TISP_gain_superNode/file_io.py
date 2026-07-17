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


def write_header_file(in0, in1, out, testCase, headerFileName):
    testId = testCase["ID"]
    dType = testCase["dType"]
    widthIn = testCase["widthIn"]
    heightIn = testCase["heightIn"]
    widthOut = testCase["widthIn"]
    heightOut = testCase["heightIn"]

    attributesIn0 = '__attribute__((section(".staticData"))) static '
    attributesIn1 = '__attribute__((section(".staticData"))) static '
    attributesOut = '__attribute__((section(".staticData"))) static '

    outputDir = "../../../../superNode/i2d_to_o2d/TISP_gain_superNode/test_data/"

    if not os.path.exists(outputDir):
        os.mkdir(outputDir)

    with open(outputDir + headerFileName, "w") as f:
        write_attributes(
            f,
            attributesIn0,
            dType + " " + "staticRefInCase" + str(testId) + "[]=\n",
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
            attributesIn1,
            dType + " " + "staticRefIn1Case" + str(testId) + "[]=\n",
        )
        for dimX in range(0, 1):
            print("{:>2}".format(in1[dimX]), file=f, end=", ")

        f.write("\n};")
        f.write("\n\n")
        f.close()

    with open(outputDir + headerFileName, "a") as f:
        write_attributes(
            f,
            attributesOut,
            dType + " " + "staticRefOutCase" + str(testId) + "[]=\n",
        )
        for dimY in range(0, heightOut):
            for dimX in range(0, widthOut):
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
