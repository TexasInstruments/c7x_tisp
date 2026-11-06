# Copyright (C) 2026 Texas Instruments Incorporated
#
# SPDX-License-Identifier: LicenseRef-TI-TFL

from __future__ import print_function

import os


def write_header_file(inp, out, testCase, headerFileName):
    """Write the input and output vectors of a test case to a header file.

    real input (numChannels x numPoints), complex output (numChannels x bins, written as interleaved re/im)
    """
    testId = testCase["ID"]

    attributes = '__attribute__((section(".staticData"))) static float '

    outputDir = "../../../FFTLIB/TISP_rfft1dBatched/test_data/"
    os.makedirs(outputDir, exist_ok=True)

    with open(outputDir + headerFileName, "w") as f:
        f.write(attributes + "staticRefInCase" + str(testId) + "[]=\n")
        f.write("{\n")
        for c in range(inp.shape[0]):
            for i in range(inp.shape[1]):
                print("{:>2}".format(inp[c, i]), file=f, end=",")
            f.write("\n")
        f.write("\n};")
        f.write("\n\n")

        f.write(attributes + "staticRefOutCase" + str(testId) + "[]=\n")
        f.write("{\n")
        for c in range(out.shape[0]):
            for i in range(out.shape[1]):
                print("{:>2}".format(out[c, i].real), file=f, end=",")
                print("{:>2}".format(out[c, i].imag), file=f, end=",")
            f.write("\n")
        f.write("\n};")
        f.write("\n\n")
