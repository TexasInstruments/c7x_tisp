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


def write_header_file(in0, params, out, testCase, headerFileName):
    testId = testCase["ID"]
    dType = testCase["dType"]
    widthIn = testCase["widthIn"]
    heightIn = testCase["heightIn"]

    attributes = '__attribute__((section(".staticData"))) static '

    outputDir = "../../../../superNode/i2d_to_o2d/TISP_downwardExpander_superNode/test_data/"

    if not os.path.exists(outputDir):
        os.mkdir(outputDir)

    # The node takes interleaved data only: rows are samples, columns are channels
    with open(outputDir + headerFileName, "w") as f:
        write_attributes(
            f,
            attributes,
            dType + " " + "staticRefInCase" + str(testId) + "[]=\n",
        )
        for dimY in range(0, widthIn):
            for dimX in range(0, heightIn):
                print("{:>2}".format(in0[dimY, dimX]), file=f, end=", ")
            print(" ", file=f)
        f.write("\n};")
        f.write("\n\n")
        f.close()

    # One array per tunable parameter, one value per channel
    paramNames = ["Threshold", "Ratio", "KneeWidth", "AttackCoeff", "ReleaseCoeff", "HoldSamples"]

    for paramName, paramValues in zip(paramNames, params):
        with open(outputDir + headerFileName, "a") as f:
            write_attributes(
                f,
                attributes,
                dType + " " + "staticRef" + paramName + "Case" + str(testId) + "[]=\n",
            )
            for dimX in range(0, heightIn):
                print("{:>2}".format(paramValues[dimX]), file=f, end=", ")

            f.write("\n};")
            f.write("\n\n")
            f.close()

    with open(outputDir + headerFileName, "a") as f:
        write_attributes(
            f,
            attributes,
            dType + " " + "staticRefOutCase" + str(testId) + "[]=\n",
        )
        for dimY in range(0, widthIn):
            for dimX in range(0, heightIn):
                print("{:>2}".format(out[dimY, dimX]), file=f, end=", ")
            print(" ", file=f)
        f.write("\n};")
        f.write("\n\n")
        f.close()
