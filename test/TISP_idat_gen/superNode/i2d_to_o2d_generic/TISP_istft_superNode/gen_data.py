#!/usr/bin/env python3
"""
ISTFT SuperNode test-data generator for TISP.

Generates complex spectral input + expected real-sample output for an ISTFT node
wrapped in a TISP::SuperNode::I2dToO2dDiffDims (the "i2d_to_o2d_generic" variant).
The SuperNode streams numBlocks input blocks DDR->L2, runs the ISTFT node on each
L2 block (its pWindow/pOlaBuf/pScratch are FIXED single-buffered L2 allocations that
persist across blocks -- the OLA state carries block-to-block, giving the streaming
result), and streams the real output samples L2->DDR.

The ISTFT input (numWindows*inBins, complex) differs in shape from its real output
(numWindows*hopSize), so the I2dToO2dDiffDims wrapper is used (1 in, 1 out, different
dims), NOT I2dToO2d.

DDR / DMA layout (matches I2dToO2dDiffDims: height=numChannels, blocks tiled along
the width):
    input  : numChannels rows x (numWindows * inBins  * numBlocks) cols  (complex interleaved)
             row c = [block0(W*inBins)  | block1(W*inBins)  | ...]
    output : numChannels rows x (numWindows * hopSize * numBlocks) cols  (real)
             row c = [block0(W*hopSize) | block1(W*hopSize) | ...]
The flattened (C-order) arrays written to the header are [ch0 row][ch1 row]...

Reference math is the per-frame irfft/ifft + synthesis window + overlap-add +
steady-state window-envelope normalization (AUDIOLIB_istft_cn / TISP_istft.cpp),
via the local istft.py.  Input frames are Hermitian-symmetric (forward-FFT of a
random real signal) so FFTLIB's split-trick IFFT matches numpy.

-----------------------------------------------------------------------------
idat CONTRACT  (define this in TISP_istft_superNode_idat.h on the C side):

    typedef struct {
       uint8_t  testPattern;        // 0..3 fill pattern / STATIC marker
       float   *staticIn;           // [numChannels][numWindows*inBins*numBlocks], complex-interleaved, or NULL
       float   *staticOut;          // [numChannels][numWindows*hopSize*numBlocks], real, or NULL
       uint32_t fftSize;
       uint32_t windowLength;
       uint32_t hopSize;
       uint32_t numChannels;
       uint32_t numWindows;         // spectral frames per block per channel
       uint32_t onesided;           // 1 = N/2+1 bins, 0 = N bins
       uint32_t windowType;         // TISP::WindowType (0=rect,1=hann,2=hamming,3=blackman,
                                    //                   4=blackman_harris,5=bartlett,6=sqrt(hann))
       uint32_t windowAlign;        // 0 = center-padded, 1 = left-aligned
       uint32_t windowPeriodic;     // 1 = periodic (DFT-even), 0 = symmetric
       uint32_t numBlocks;          // number of SuperNode-streamed blocks
       uint32_t dataPitchIn;        // DDR input  row pitch in BYTES = W*inBins*numBlocks*sizeof(float)
       uint32_t dataPitchOut;       // DDR output row pitch in BYTES = W*hopSize*numBlocks*sizeof(float)
       uint32_t outputDataLocation; // 0 -> HEAP, 1 -> MSMC
       uint32_t numReps;
       uint32_t testID;
    } TISP_istft_superNode_testParams_t;

    void TISP_istft_superNode_getTestParams(TISP_istft_superNode_testParams_t **params, int32_t *numTests);

The C test uses inBins = onesided ? fftSize+2 : 2*fftSize and builds I2dToO2dDiffDims:
    widthIn  = W*inBins*numBlocks,  heightIn  = numChannels, blockWidthIn  = W*inBins
    widthOut = W*hopSize*numBlocks, heightOut = numChannels, blockWidthOut = W*hopSize
    inPitch  = dataPitchIn,         outPitch  = dataPitchOut, blockHeight* = numChannels
ISTFT scratch = TISP::AUDIOLIB::Istft<float>::calcScratchSize(fftSize, numWindows);
OLA buffer    = TISP::AUDIOLIB::Istft<float>::calcOlaBufSize(fftSize, hopSize) * numChannels.
-----------------------------------------------------------------------------
"""

from __future__ import print_function
from subprocess import call
from sys import platform

import sys
import csv
import argparse
import numpy as np

# common helpers live in TISP_idat_gen/common (gen_ifdefs etc.)
sys.path.append("../../../common")
import TISP_utils

import file_io
from istft import WINDOW_TYPE_ENUM, generate_padded_window, istft_reference, generate_hermitian_input

# Maps CSV windowAlign string to the C-side enum (0 = center-padded, 1 = left-aligned)
WINDOW_ALIGN_ENUM = {"center": 0, "left": 1}

FLOAT_BYTES = 4

IDAT_FILE = "../../../../superNode/i2d_to_o2d_generic/TISP_istft_superNode/TISP_istft_superNode_idat.c"
IDAT_HEADER_INCLUDE = '#include "TISP_istft_superNode_idat.h"'
STRUCT_NAME = "TISP_istft_superNode_testParams_t"


def in_bins_of(fftSize, onesided):
    return (fftSize + 2) if onesided else (2 * fftSize)


def gen_testCaseParams(testParamsFile, currPrm):
    """Write one TISP_istft_superNode_testParams_t initializer to the idat file."""
    testId = currPrm["ID"]
    fftSize = currPrm["fftSize"]
    windowLength = currPrm["windowLength"]
    hopSize = currPrm["hopSize"]
    numChannels = currPrm["numChannels"]
    numWindows = currPrm["numWindows"]
    onesided = currPrm["onesided"]
    testType = currPrm["testType"]
    demoCase = currPrm["demoCase"]
    outputDataLocation = currPrm["outputDataLocation"]
    numBlocks = currPrm["numBlocks"]
    numReps = currPrm["numReps"]
    windowTypeStr = currPrm.get("windowType", "hann")
    windowTypeEnum = WINDOW_TYPE_ENUM[windowTypeStr]
    windowAlignStr = currPrm.get("windowAlign", "center")
    windowAlignEnum = WINDOW_ALIGN_ENUM[windowAlignStr]
    windowPeriodic = currPrm.get("windowPeriodic", True)
    windowPeriodicVal = 1 if windowPeriodic else 0

    inBins = in_bins_of(fftSize, onesided != 0)
    dataPitchIn = numWindows * inBins * numBlocks * FLOAT_BYTES
    dataPitchOut = numWindows * hopSize * numBlocks * FLOAT_BYTES

    if testType in ["STATIC", "SINE"]:
        staticIn = "staticRefInCase" + str(testId)
        staticOut = "staticRefOutCase" + str(testId)
        testType = "STATIC"
    else:
        staticIn = "NULL"
        staticOut = "NULL"

    if demoCase == "TRUE":
        testParamsFile.write(
            "#if (defined(ALL_TEST_CASES) || (TEST_CASE == %d) || defined(DEMO_CASE))\n"
            % (int(testId))
        )
    else:
        testParamsFile.write(
            "#if (defined(ALL_TEST_CASES) || (TEST_CASE == %d))\n" % (int(testId))
        )

    testParamsFile.write("{\n")
    testParamsFile.write("%s, // testPattern \n" % testType)
    testParamsFile.write("(float *) %s, // *staticIn \n" % staticIn)
    testParamsFile.write("(float *) %s, // *staticOut \n" % staticOut)
    testParamsFile.write("%s, // fftSize \n" % fftSize)
    testParamsFile.write("%s, // windowLength \n" % windowLength)
    testParamsFile.write("%s, // hopSize \n" % hopSize)
    testParamsFile.write("%s, // numChannels \n" % numChannels)
    testParamsFile.write("%s, // numWindows \n" % numWindows)
    testParamsFile.write("%s, // onesided \n" % onesided)
    testParamsFile.write("%d, // windowType (%s) \n" % (windowTypeEnum, windowTypeStr))
    testParamsFile.write("%d, // windowAlign (%s) \n" % (windowAlignEnum, windowAlignStr))
    testParamsFile.write(
        "%d, // windowPeriodic (%s) \n"
        % (windowPeriodicVal, "periodic" if windowPeriodic else "symmetric")
    )
    testParamsFile.write("%d, // numBlocks \n" % int(numBlocks))
    testParamsFile.write("%d, // dataPitchIn (bytes) \n" % dataPitchIn)
    testParamsFile.write("%d, // dataPitchOut (bytes) \n" % dataPitchOut)
    testParamsFile.write("%s, // outputDataLocation \n" % outputDataLocation)
    testParamsFile.write("%d, // numReps \n" % int(numReps))
    testParamsFile.write("%d, // testID \n" % int(testId))
    testParamsFile.write("},\n")
    testParamsFile.write("#endif\n")
    testParamsFile.write("\n")


def gen_idat_file(idatFile, testCases):
    """Generate the idat .c file."""
    fIn = open("../../../common/ti_lice_header_idat.txt", "r")
    lines = fIn.readlines()
    fIn.close()
    idatFile.writelines(lines)
    idatFile.writelines(IDAT_HEADER_INCLUDE)
    idatFile.write("\n")
    idatFile.write("/*This file is autogenerated*/\n")
    idatFile.write("/*Please do not edit*/\n")
    idatFile.write("\n\n")

    for testCase in testCases:
        if testCase["testType"] in ["STATIC", "SINE"]:
            TISP_utils.gen_ifdefs(idatFile, testCase)

    idatFile.write("static %s testParams[] =  " % STRUCT_NAME)
    idatFile.write("{\n\n")

    for testCase in testCases:
        gen_testCaseParams(idatFile, testCase)

    idatFile.write("};\n\n")

    fIn = open("test_param_function.txt", "r")
    lines = fIn.readlines()
    idatFile.writelines(lines)


def process_test_case(testCase):
    """Convert CSV strings to the right types."""
    testCase["fftSize"] = int(testCase["fftSize"])
    testCase["windowLength"] = int(testCase.get("windowLength", testCase["fftSize"]))
    testCase["hopSize"] = int(testCase["hopSize"])
    testCase["numChannels"] = int(testCase["numChannels"])
    testCase["numWindows"] = int(testCase["numWindows"])
    testCase["onesided"] = int(testCase["onesided"])
    testCase["numBlocks"] = int(testCase.get("numBlocks", 1))
    testCase["numReps"] = int(testCase.get("numReps", 1))
    testCase["demoCase"] = testCase.get("demoCase", "FALSE").upper()
    testCase["testType"] = testCase.get("testType", "STATIC").upper()
    testCase["outputDataLocation"] = testCase.get(
        "outputDataLocation", "AUDIOLIB_TEST_OUTPUT_MSMC"
    )
    testCase["windowType"] = testCase.get("windowType", "hann").strip().lower()
    testCase["windowAlign"] = testCase.get("windowAlign", "center").strip().lower()
    testCase["windowPeriodic"] = (
        str(testCase.get("windowPeriodic", "TRUE")).strip().upper() == "TRUE"
    )
    return testCase


def gen_test_case_header_file(testCase):
    """Generate the per-case input/output header in the SuperNode 2D DMA layout.

    input  : numChannels rows x (W*inBins*numBlocks) cols  (complex-interleaved)
    output : numChannels rows x (W*hopSize*numBlocks) cols (real)
    Each channel's row is a self-contained [block0 | block1 | ...] stream; running
    the OLA reference on it (num_channels=1, num_blocks=numBlocks) carries the OLA
    state block-to-block -- the streaming-equivalent result.
    """
    testId = int(testCase["ID"])
    fftSize = testCase["fftSize"]
    windowLength = testCase["windowLength"]
    hopSize = testCase["hopSize"]
    numChannels = testCase["numChannels"]
    numWindows = testCase["numWindows"]
    onesided = testCase["onesided"] != 0
    numBlocks = testCase["numBlocks"]
    windowTypeStr = testCase.get("windowType", "hann")
    windowAlignStr = testCase.get("windowAlign", "center")
    windowPeriodic = testCase.get("windowPeriodic", True)

    np.random.seed(42 + testId)

    window = generate_padded_window(windowTypeStr, windowLength, fftSize, windowAlignStr, windowPeriodic)
    inBins = in_bins_of(fftSize, onesided)

    rowInLen = numWindows * inBins * numBlocks
    rowOutLen = numWindows * hopSize * numBlocks

    x = np.zeros((numChannels, rowInLen), dtype=np.float32)
    y = np.zeros((numChannels, rowOutLen), dtype=np.float32)

    for chIdx in range(numChannels):
        # per-channel row = numBlocks*numWindows Hermitian-symmetric frames
        xRow = generate_hermitian_input(
            fftSize, onesided, numBlocks * numWindows,
            lambda count: np.random.uniform(-1.0, 1.0, count).astype(np.float32),
        )
        # OLA reference for one channel, numBlocks blocks (state carries across blocks)
        yRow = istft_reference(
            xRow, window, fftSize, hopSize, 1, numWindows, onesided, numBlocks
        )
        x[chIdx, :] = xRow
        y[chIdx, :] = yRow

    headerFileName = "staticRefCase" + str(testId) + ".h"
    # flatten C-order -> [ch0 row][ch1 row]... (matches the I2dToO2dDiffDims walk)
    file_io.write_header_file(x.reshape(-1), y.reshape(-1), testCase, headerFileName)

    print(
        "Test case %d: fftSize=%d windowLength=%d hopSize=%d numCh=%d numWindows=%d inBins=%d "
        "onesided=%d numBlocks=%d windowType=%s align=%s periodic=%s => input=%d output=%d floats"
        % (
            testId, fftSize, windowLength, hopSize, numChannels, numWindows, inBins,
            int(onesided), numBlocks, windowTypeStr, windowAlignStr, windowPeriodic, x.size, y.size,
        )
    )


parser = argparse.ArgumentParser()
parser.add_argument("-a", "--allCases", action="store_true", help="Generate all test cases")
myArgs = parser.parse_args()

testCasesCsvFile = "test_cases_list.csv"
testIdList = []
testIdList += range(0, 20)


def gen_test_case(testCase):
    if testCase["testType"] in ["STATIC", "SINE"]:
        gen_test_case_header_file(testCase)
    else:
        print(
            "Skipping test case generation for test ID",
            testCase["ID"],
            "as testType is RANDOM",
        )


with open(testCasesCsvFile, encoding="utf-8-sig") as csv_file:
    testCaseReader = csv.DictReader(csv_file)
    testCases = list(testCaseReader)

    for testCase in testCases:
        testCase = process_test_case(testCase)

    for testCase in testCases:
        if myArgs.allCases is True:
            gen_test_case(testCase)
        elif int(testCase["ID"]) in testIdList:
            gen_test_case(testCase)


with open(IDAT_FILE, "w") as idatFile:
    gen_idat_file(idatFile, testCases)


if platform == "linux" or platform == "linux2":
    call(["clang-format", "-style=file", "-i", IDAT_FILE])
    call(["indent", "-nut", "-i3", "-c55", "-l200", IDAT_FILE])
    call(["rm", IDAT_FILE + "~"])


print("\nIdat file generation completed")
