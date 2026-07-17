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


def write_header_file(inputData, filterData, outputData, testCase, headerFileName):
    testId = testCase["ID"]
    dTypeIn = testCase["dTypeIn"]
    dTypeFilter = testCase["dTypeFilter"]
    dTypeOut = testCase["dTypeOut"]
    dataSize = testCase["dataSize"]
    filterSize = testCase["filterSize"]
    batchSize = testCase["batchSize"]
    outSize = dataSize
    numBlocks = testCase["numBlocks"]
    enableNChCoefs = testCase["enableNChCoefs"]

    filterHeight = 1 if enableNChCoefs == 0 else batchSize

    attributesIn = '__attribute__((section(".staticData"))) static '
    attributesFilter = '__attribute__((section(".staticData"))) static '
    attributesOut = '__attribute__((section(".staticData"))) static '

    outputDir = "../../../DSPLIB/TISP_fir/test_data/"

    if not os.path.exists(outputDir):
        os.mkdir(outputDir)

    with open(outputDir + headerFileName, "w") as f:
        write_attributes(
            f,
            attributesIn,
            dTypeIn + " " + "staticRefInCase" + str(testId) + "[]=\n",
        )

        for dimY in range(0, batchSize):
            for dimX in range(0, (dataSize * numBlocks)):
                print("{:>2}".format(inputData[dimY][dimX]), file=f, end=", ")
            f.write("\n\n")
        f.write("\n};")
        f.write("\n\n")
        f.close()

    with open(outputDir + headerFileName, "a") as f:
        write_attributes(
            f,
            attributesFilter,
            dTypeIn + " " + "staticRefFilterCase" + str(testId) + "[]=\n",
        )

        for dimY in range(0, filterHeight):
            for dimX in range(0, filterSize):
                print("{:>2}".format(filterData[dimY][dimX]), file=f, end=", ")
            f.write("\n\n")
        f.write("\n};")
        f.write("\n\n")
        f.close()

    with open(outputDir + headerFileName, "a") as f:
        write_attributes(
            f,
            attributesOut,
            dTypeIn + " " + "staticRefOutCase" + str(testId) + "[]=\n",
        )

        for dimY in range(0, batchSize):
            for dimX in range(0, (numBlocks * outSize)):
                print("{:>2}".format(outputData[dimY][dimX]), file=f, end=", ")
            f.write("\n\n")
        f.write("\n};")
        f.write("\n\n")
        f.close()

    # if platform == "linux" or platform == "linux2":
    #     call(
    #         [
    #             "indent",
    #             "-nut",
    #             "-i3",
    #             # "-c55",
    #             "-l100",
    #             outputDir + headerFileName,
    #         ]
    #     )
    #     call(["rm", outputDir + headerFileName + "~"])  # remove backup file
