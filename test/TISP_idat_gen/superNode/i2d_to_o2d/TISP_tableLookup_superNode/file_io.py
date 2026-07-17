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


def write_header_file(in0, in1, out, testCase, headerFileName):

    testId = testCase["ID"]
    srcSamples = testCase["srcSamples"]
    tableSamples = testCase["tableSamples"]
    dType = testCase["dType"]

    attributesIn0 = '__attribute__((section(".staticData"))) static '
    attributesIn1 = '__attribute__((section(".staticData"))) static '
    attributesout = '__attribute__((section(".staticData"))) static '

    outputDir = "../../../../superNode/i2d_to_o2d/TISP_tableLookup_superNode/test_data/"

    if not os.path.exists(outputDir):
        os.mkdir(outputDir)

    with open(outputDir + headerFileName, "w") as f:
        write_attributes(
            f,
            attributesIn0,
            dType + " " + f"staticRefIn0Case" + str(testId) + "[]=\n",
        )

        for dimY in range(0, srcSamples):
            print("{:>2}".format(in0[dimY]), file=f, end=", ")
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

        for dimY in range(0, tableSamples):
            print("{:>2}".format(in1[dimY]), file=f, end=", ")
        print(" ", file=f)

        f.write("\n};")

        f.write("\n\n")
        f.close()
    with open(outputDir + headerFileName, "a") as f:
        write_attributes(
            f,
            attributesout,
            dType + " " + f"staticRefOutCase" + str(testId) + "[]=\n",
        )

        for dimY in range(0, srcSamples):
            print("{:>2}".format(out[dimY]), file=f, end=", ")
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
