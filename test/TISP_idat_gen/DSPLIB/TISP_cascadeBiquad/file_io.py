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
    inputData,
    sosCoefs,
    filterStateInit,
    outputData,
    filterStateFinal,
    tifBiquadCascadeCoefs,
    tifBiquadCascadeGains,
    testCase,
    headerFileName,
):
    testId = testCase["ID"]
    dataType = testCase["dataType"]
    biquadForm = testCase["biquadForm"]
    enableNChCoeffs = testCase["enableNChCoeffs"]

    attributesIn = '__attribute__((section(".staticData"))) static '
    attributesFilterCoef = '__attribute__((section(".staticData"))) static '
    attributesFilterVar = (
        '__attribute__((section(".staticData"))) __attribute__((unused)) static '
    )
    attributesOut = '__attribute__((section(".staticData"))) static '

    outputDir = "../../../DSPLIB/TISP_cascadeBiquad/test_data/"

    if not os.path.exists(outputDir):
        os.mkdir(outputDir)

    with open(outputDir + headerFileName, "w") as f:
        write_attributes(
            f,
            attributesIn,
            dataType + " " + "staticRefInCase" + str(testId) + "[]=\n",
        )
        dataSize = inputData.shape[0]
        numChannels = inputData.shape[1]
        for dimX in range(0, dataSize):
            for dimY in range(0, numChannels):
                print("{:>2}".format(inputData[dimX][dimY]), file=f, end=", ")
            f.write("\n\n")
        f.write("\n};")
        f.write("\n\n")
        f.close()

    with open(outputDir + headerFileName, "a") as f:
        write_attributes(
            f,
            attributesFilterCoef,
            dataType + " " + "staticRefFilterCoefCase" + str(testId) + "[]=\n",
        )
        numChannels = sosCoefs.shape[2]
        numStages = sosCoefs.shape[0]
        for dimZ in range(0, numChannels):
            for dimX in range(0, numStages):
                coeffs = sosCoefs[
                    dimX, [0, 1, 2, 4, 5], dimZ
                ]  # Exclude a0=1.0 coefficient at index 3
                for coeff in coeffs:
                    print("{:>2}".format(coeff), file=f, end=", ")
                f.write("\n\n")
        f.write("\n};")
        f.write("\n\n")
        f.close()

    with open(outputDir + headerFileName, "a") as f:
        write_attributes(
            f,
            attributesFilterVar,
            dataType + " " + "staticRefFilterStateInitCase" + str(testId) + "[]=\n",
        )
        numStages = filterStateInit.shape[0]
        numStateVars = filterStateInit.shape[1]
        numChannels = filterStateInit.shape[2]
        for dimX in range(0, numStages):
            for dimY in range(0, numStateVars):
                for dimZ in range(0, numChannels):
                    print(
                        "{:>2}".format(filterStateInit[dimX][dimY][dimZ]),
                        file=f,
                        end=", ",
                    )
            f.write("\n\n")
        f.write("\n};")
        f.write("\n\n")
        f.close()

    with open(outputDir + headerFileName, "a") as f:
        write_attributes(
            f,
            attributesOut,
            dataType + " " + "staticRefOutCase" + str(testId) + "[]=\n",
        )
        dataSize = inputData.shape[0]
        numChannels = inputData.shape[1]
        for dimX in range(0, dataSize):
            for dimY in range(0, numChannels):
                print("{:>2}".format(outputData[dimX][dimY]), file=f, end=", ")
            f.write("\n\n")
        f.write("\n};")
        f.write("\n\n")
        f.close()

    with open(outputDir + headerFileName, "a") as f:
        write_attributes(
            f,
            attributesFilterVar,
            dataType + " " + "staticRefFilterStateFinalCase" + str(testId) + "[]=\n",
        )
        numStages = filterStateInit.shape[0]
        numStateVars = filterStateInit.shape[1]
        numChannels = filterStateInit.shape[2]
        for dimX in range(0, numStages):
            for dimY in range(0, numStateVars):
                for dimZ in range(0, numChannels):
                    print(
                        "{:>2}".format(filterStateFinal[dimX][dimY][dimZ]),
                        file=f,
                        end=", ",
                    )
            f.write("\n\n")
        f.write("\n};")
        f.write("\n\n")
        f.close()

    if biquadForm == "DSPLIB_BIQUAD_TIF":
        assert (
            tifBiquadCascadeCoefs is not None and tifBiquadCascadeGains is not None
        ), "Both tifBiquadCascadeCoefs and tifBiquadCascadeGains must be not None for DSPLIB_BIQUAD_TIF"

        if enableNChCoeffs == "False":
            with open(outputDir + headerFileName, "a") as f:
                write_attributes(
                    f,
                    attributesFilterCoef,
                    dataType
                    + " "
                    + "staticRefFilterCoefOptCase"
                    + str(testId)
                    + "[]=\n",
                )
                numStages = tifBiquadCascadeCoefs.shape[0]
                for dimX in range(0, numStages):
                    for dimY in [0, 1, 3, 2]:
                        print(
                            "{:>2}".format(tifBiquadCascadeCoefs[dimX][dimY][0]),
                            file=f,
                            end=", ",
                        )
                    f.write("\n\n")
                numStageGroups = tifBiquadCascadeGains.shape[0]
                for dimX in range(0, numStageGroups):
                    print(
                        "{:>2}".format(tifBiquadCascadeGains[dimX][0]), file=f, end=", "
                    )
                f.write("\n\n")
                f.write("\n};")
                f.write("\n\n")
                f.close()

        else:
            with open(outputDir + headerFileName, "a") as f:
                write_attributes(
                    f,
                    attributesFilterCoef,
                    dataType
                    + " "
                    + "staticRefFilterCoefOptCase"
                    + str(testId)
                    + "[]=\n",
                )
                numChannels = tifBiquadCascadeCoefs.shape[2]
                numStages = tifBiquadCascadeCoefs.shape[0]
                for dimY in [0, 1, 3, 2]:
                    for dimX in range(0, numStages):
                        for dimZ in range(0, numChannels):
                            print(
                                "{:>2}".format(tifBiquadCascadeCoefs[dimX][dimY][dimZ]),
                                file=f,
                                end=", ",
                            )
                        f.write("\n\n")
                numStageGroups = tifBiquadCascadeGains.shape[0]
                for dimX in range(0, numStageGroups):
                    for dimY in range(0, numChannels):
                        print(
                            "{:>2}".format(tifBiquadCascadeGains[dimX][dimY]),
                            file=f,
                            end=", ",
                        )
                f.write("\n\n")
                f.write("\n};")
                f.write("\n\n")
                f.close()

    elif biquadForm in ["DSPLIB_BIQUAD_DF1", "DSPLIB_BIQUAD_DF2", "DSPLIB_BIQUAD_TDF2"]:
        if enableNChCoeffs == "False":
            pass  # Do nothing
        if enableNChCoeffs == "True":
            with open(outputDir + headerFileName, "a") as f:
                # Tranpose coefficients for optimized code
                write_attributes(
                    f,
                    attributesFilterCoef,
                    dataType
                    + " "
                    + "staticRefFilterCoefOptCase"
                    + str(testId)
                    + "[]=\n",
                )
                numChannels = sosCoefs.shape[2]
                numStages = sosCoefs.shape[0]
                for dimX in range(0, numStages):
                    for dimY in [0, 1, 2, 4, 5]:
                        for dimZ in range(0, numChannels):
                            print(
                                "{:>2}".format(sosCoefs[dimX][dimY][dimZ]),
                                file=f,
                                end=", ",
                            )
                        f.write("\n\n")
                f.write("\n};")
                f.write("\n\n")
                f.close()

    else:
        raise ValueError("Unsupported biquadForm: " + biquadForm)

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
