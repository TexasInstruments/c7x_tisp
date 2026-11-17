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


def write_header_file(in0, in1, out0, testCase, headerFileName):
    testId = testCase["ID"]
    dType = testCase["dType"]
    inWidth = testCase["inWidth"]
    inHeight = testCase["inHeight"]
    strideIn = testCase["strideIn"]
    numRows = testCase["numRows"]
    numCols = testCase["numCols"]

    attributesIn0 = '__attribute__((section(".staticData"))) static '
    attributesIn1 = '__attribute__((section(".staticData"))) static '
    attributesOut0 = '__attribute__((section(".staticData"))) static '

    outputDir = "../../../../superNode/i2d_to_o2d_generic/TISP_tableSourceRowColSelector_superNode/test_data/"

    if not os.path.exists(outputDir):
        os.mkdir(outputDir)

    with open(outputDir + headerFileName, "w") as f:
        write_attributes(
            f,
            attributesIn0,
            "int32_t" + " " + "staticRefIn0Case" + str(testId) + "[]=\n",
        )
        # The supernode reads this array directly, so each row of inWidth indices is padded to strideIn
        for dimY in range(0, inHeight):
            for dimX in range(0, strideIn):
                value = in0[dimY * inWidth + dimX] if dimX < inWidth else 0
                print("{:>2}".format(value), file=f, end=", ")
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
        for dimY in range(0, numRows):
            for dimX in range(0, numCols):
                print("{:>2}".format(in1[dimY, dimX]), file=f, end=", ")
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
        for dimY in range(0, out0.shape[0]):
            for dimX in range(0, out0.shape[1]):
                print("{:>2}".format(out0[dimY, dimX]), file=f, end=", ")
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
