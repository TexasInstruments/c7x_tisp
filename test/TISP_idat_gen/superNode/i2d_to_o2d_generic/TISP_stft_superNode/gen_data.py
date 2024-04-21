#!/usr/bin/env python3
"""
STFT SuperNode test-data generator for TISP.

Generates input + expected output for an STFT node wrapped in a
TISP::SuperNode::I2dToO2dDiffDims (the "i2d_to_o2d_generic" variant), where the
SuperNode streams numBlocks input blocks DDR->L2, runs the STFT node on each
L2 block (its pState/pScratch/pWindow are FIXED single-buffered L2 allocations
that persist across blocks, exactly like the FIR SuperNode's state/filter),
and streams the complex output frames L2->DDR.

The STFT output (numWindows*outBins, complex) differs in shape from its real
input (numInputSamples), so the I2dToO2dDiffDims wrapper is used (1 in, 1 out,
different dims), NOT I2dToO2d.

DDR / DMA layout (matches I2dToO2dDiffDims: height=numChannels, blocks tiled
along the width):
    input  : numChannels rows x (numInputSamples * numBlocks) cols
             row c = [block0(F) | block1(F) | ...]
    output : numChannels rows x (numWindows * outBins * numBlocks) cols
             row c = [block0(W*outBins) | block1(W*outBins) | ...]
The flattened (C-order) arrays written to the header are therefore
[ch0 row][ch1 row]..., i.e. channel-major with blocks along the row.

Reference math is torch.stft(center=False) via the local stft.py (identical to
AUDIOLIB/TISP_stft).  Window coefficients are produced on target by
TISP::windowGen(); only the windowType / windowAlign / windowPeriodic enum
values are baked into the idat.

-----------------------------------------------------------------------------
idat CONTRACT  (define this in TISP_stft_superNode_idat.h on the C side):

    typedef struct {
       uint8_t  testPattern;        // 0..3 fill pattern / STATIC marker
       float   *staticIn;           // [numChannels][numInputSamples*numBlocks], row-major, or NULL
       float   *staticOut;          // [numChannels][numWindows*outBins*numBlocks], row-major, or NULL
       uint32_t fftSize;
       uint32_t windowLength;
       uint32_t hopSize;
       uint32_t numChannels;
       uint32_t numInputSamples;    // F per block per channel
       uint32_t onesided;           // 1 = N/2+1 bins, 0 = N bins
       uint32_t windowType;         // TISP::WindowType (0=rect,1=hann,2=hamming,3=blackman,
                                    //                   4=blackman_harris,5=bartlett,6=sqrt(hann))
       uint32_t windowAlign;        // 0 = center-padded, 1 = left-aligned
       uint32_t windowPeriodic;     // 1 = periodic (DFT-even), 0 = symmetric
       uint32_t numBlocks;          // number of SuperNode-streamed blocks
       uint32_t dataPitchIn;        // DDR input  row pitch in BYTES = F*numBlocks*sizeof(float)
       uint32_t dataPitchOut;       // DDR output row pitch in BYTES = W*outBins*numBlocks*sizeof(float)
       uint32_t outputDataLocation; // 0 -> HEAP, 1 -> MSMC
       uint32_t numReps;
       uint32_t testID;
    } TISP_stft_superNode_testParams_t;

    void TISP_stft_superNode_getTestParams(TISP_stft_superNode_testParams_t **params, int32_t *numTests);

The C test computes numWindows via TISP::AUDIOLIB::Stft::calcNumWindows(fftSize,
hopSize, numInputSamples) and outBins = onesided ? fftSize+2 : 2*fftSize, then
builds I2dToO2dDiffDims with:
    widthIn  = F*numBlocks,            heightIn  = numChannels,  blockWidthIn  = F
    widthOut = W*outBins*numBlocks,    heightOut = numChannels,  blockWidthOut = W*outBins
    inPitch  = dataPitchIn,            outPitch  = dataPitchOut,  blockHeight* = numChannels
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
from stft import WINDOW_TYPE_ENUM, generate_window, stft_reference_torch

# Maps CSV windowAlign string to the C-side enum (0 = center-padded, 1 = left-aligned)
WINDOW_ALIGN_ENUM = {"center": 0, "left": 1}

FLOAT_BYTES = 4

IDAT_FILE = "../../../../superNode/i2d_to_o2d_generic/TISP_stft_superNode/TISP_stft_superNode_idat.c"
IDAT_HEADER_INCLUDE = '#include "TISP_stft_superNode_idat.h"'
STRUCT_NAME = "TISP_stft_superNode_testParams_t"

def derived_dims(fftSize, hopSize, numInputSamples, onesided):
    """numWindows / outBins for numInputSamples >= fftSize (standard streaming formula)."""
    n, h, f = fftSize, hopSize, numInputSamples
    firstFrameWindows = (f - n) // h + 1
    overlapBufSize = f - firstFrameWindows * h
    numWindows = (overlapBufSize + f - n) // h + 1
    outBins = (n + 2) if onesided else (2 * n)
    return numWindows, outBins


def gen_testCaseParams(testParamsFile, currPrm):
    """Write one TISP_stft_superNode_testParams_t initializer to the idat file."""
    testId = currPrm["ID"]
    fftSize = currPrm["fftSize"]
    windowLength = currPrm["windowLength"]
    hopSize = currPrm["hopSize"]
    numChannels = currPrm["numChannels"]
    numInputSamples = currPrm["numInputSamples"]
    onesided = currPrm["onesided"]
    testType = currPrm["testType"]
    demoCase = currPrm["demoCase"]
    outputDataLocation = currPrm["outputDataLocation"]
    numBlocks = currPrm["numBlocks"]
    numReps = currPrm["numReps"]
    C7100 = currPrm["C7100"]
    windowTypeStr = currPrm.get("windowType", "hann")
    windowTypeEnum = WINDOW_TYPE_ENUM[windowTypeStr]
    windowAlignStr = currPrm.get("windowAlign", "center")
    windowAlignEnum = WINDOW_ALIGN_ENUM[windowAlignStr]
    windowPeriodic = currPrm.get("windowPeriodic", True)
    windowPeriodicVal = 1 if windowPeriodic else 0

    numWindows, outBins = derived_dims(fftSize, hopSize, numInputSamples, onesided != 0)
    dataPitchIn = numInputSamples * numBlocks * FLOAT_BYTES
    dataPitchOut = numWindows * outBins * numBlocks * FLOAT_BYTES

    if testType in ["STATIC", "SINE"]:
        staticIn = "staticRefInCase" + str(testId)
        staticOut = "staticRefOutCase" + str(testId)
        testType = "STATIC"
    else:
        staticIn = "NULL"
        staticOut = "NULL"

    if C7100 == "FALSE":
        testParamsFile.write("#if !defined(__C7100__)\n")

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
    testParamsFile.write("%s, // numInputSamples \n" % numInputSamples)
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

    if C7100 == "FALSE":
        testParamsFile.write("#endif // if !defined(__C7100__)\n")

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
    testCase["numInputSamples"] = int(testCase["numInputSamples"])
    testCase["onesided"] = int(testCase["onesided"])
    testCase["numBlocks"] = int(testCase.get("numBlocks", 1))
    testCase["numReps"] = int(testCase.get("numReps", 1))
    testCase["demoCase"] = testCase.get("demoCase", "FALSE").upper()
    testCase["testType"] = testCase.get("testType", "STATIC").upper()
    testCase["outputDataLocation"] = testCase.get(
        "outputDataLocation", "AUDIOLIB_TEST_OUTPUT_MSMC"
    )
    testCase["C7100"] = testCase.get("C7100", "TRUE").upper()
    testCase["windowType"] = testCase.get("windowType", "hann").strip().lower()
    testCase["windowAlign"] = testCase.get("windowAlign", "center").strip().lower()
    testCase["windowPeriodic"] = (
        str(testCase.get("windowPeriodic", "TRUE")).strip().upper() == "TRUE"
    )
    return testCase


def gen_test_case_header_file(testCase):
    """Generate the per-case input/output header in the SuperNode 2D DMA layout.

    input  : numChannels rows x (F*numBlocks) cols, row c = concatenated blocks
    output : numChannels rows x (W*outBins*numBlocks) cols
    Each channel's full (zero-state + numBlocks*F) signal is run through
    torch.stft, yielding numBlocks*W frames -- the streaming-equivalent result.
    """
    testId = int(testCase["ID"])
    fftSize = testCase["fftSize"]
    windowLength = testCase["windowLength"]
    hopSize = testCase["hopSize"]
    numChannels = testCase["numChannels"]
    numInputSamples = testCase["numInputSamples"]
    onesided = testCase["onesided"] != 0
    numBlocks = testCase["numBlocks"]
    windowTypeStr = testCase.get("windowType", "hann")
    windowAlignStr = testCase.get("windowAlign", "center")
    windowPeriodic = testCase.get("windowPeriodic", True)

    np.random.seed(42 + testId)

    window = generate_window(windowTypeStr, windowLength, windowPeriodic)
    numWindows, outBins = derived_dims(fftSize, hopSize, numInputSamples, onesided)

    rowInLen = numInputSamples * numBlocks
    rowOutLen = numWindows * outBins * numBlocks

    # channel-major rows; blocks tiled along the row width
    x = np.random.uniform(-1.0, 1.0, (numChannels, rowInLen)).astype(np.float32)
    y = np.zeros((numChannels, rowOutLen), dtype=np.float32)

    for chIdx in range(numChannels):
        # per-channel signal is already the contiguous [block0 F | block1 F | ...] row
        y[chIdx, :] = stft_reference_torch(
            x[chIdx, :], window, fftSize, windowLength, hopSize,
            numInputSamples, onesided, windowAlignStr
        )

    headerFileName = "staticRefCase" + str(testId) + ".h"
    # flatten C-order -> [ch0 row][ch1 row]... (matches the I2dToO2dDiffDims walk)
    file_io.write_header_file(x.reshape(-1), y.reshape(-1), testCase, headerFileName)

    print(
        "Test case %d: fftSize=%d windowLength=%d hopSize=%d numCh=%d numInputSamples=%d "
        "numWindows=%d outBins=%d onesided=%d numBlocks=%d windowType=%s align=%s periodic=%s "
        "=> input=%d output=%d floats"
        % (
            testId, fftSize, windowLength, hopSize, numChannels, numInputSamples,
            numWindows, outBins, int(onesided), numBlocks, windowTypeStr,
            windowAlignStr, windowPeriodic, x.size, y.size,
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
