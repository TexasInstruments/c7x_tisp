# Copyright (C) 2026 Texas Instruments Incorporated
#
# SPDX-License-Identifier: LicenseRef-TI-TFL

from __future__ import print_function

import os


def write_attributes(f, attributes, stringToWrite):
    f.write("#ifdef WIN32\n")
    f.write(stringToWrite)
    f.write("#else\n")
    f.write(attributes + stringToWrite)
    f.write("#endif\n")
    f.write("{\n")


def write_header_file(inp, out, testCase, headerFileName):
    """Write the input and output matrices of a test case to a header file.

    Conventions (per block; numBlocks blocks are stacked):
      dimX = width (columns), dimY = height (rows)
      input shape = (dimY, dimX), output shape = (dimX, dimY)
    """
    testId = testCase["ID"]
    dimX = int(testCase["dimX"])
    dimY = int(testCase["dimY"])
    dType = testCase["dType"]

    numBlocks = int(testCase.get("numBlocks", 1))

    # numBlocks matrices stacked back to back
    assert inp.shape == (numBlocks * dimY, dimX), f"input shape {inp.shape} != {(numBlocks * dimY, dimX)}"
    assert out.shape == (numBlocks * dimX, dimY), f"output shape {out.shape} != {(numBlocks * dimX, dimY)}"

    attributes = '__attribute__((section(".staticData"))) static '

    outputDir = "../../../DSPLIB/TISP_matTrans/test_data/"
    os.makedirs(outputDir, exist_ok=True)

    with open(outputDir + headerFileName, "w") as f:
        write_attributes(f, attributes, f"{dType} staticRefInCase{testId}[]=\n")
        for y in range(inp.shape[0]):
            for x in range(inp.shape[1]):
                print("{:>2}".format(inp[y, x]), file=f, end=", ")
            print(" ", file=f)
        f.write("\n};\n\n")

        write_attributes(f, attributes, f"{dType} staticRefOutCase{testId}[]=\n")
        for y in range(out.shape[0]):
            for x in range(out.shape[1]):
                print("{:>2}".format(out[y, x]), file=f, end=", ")
            print(" ", file=f)
        f.write("\n};\n\n")
