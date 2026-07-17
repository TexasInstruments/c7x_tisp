#!/usr/bin/env python3
"""
LogMelSpectrum SuperNode test-data generator for TISP.

Generates input + expected output for a LogMelSpectrum node wrapped in a
TISP::SuperNode::I2dToO2dDiffDims (the "i2d_to_o2d_generic" variant). The
SuperNode streams numBlocks input blocks DDR->L2, runs the LogMelSpectrum node
on each L2 block (its pWindow/pMel/pState/pScratch are FIXED single-buffered L2
allocations that persist across blocks), and streams the real log-mel output
frames L2->DDR.

The log-mel output (numWindows*numMelBins, real) differs in shape from its real
input (numInputSamples), so I2dToO2dDiffDims (1 in, 1 out, different dims) is used.

DDR / DMA layout (matches I2dToO2dDiffDims: height=numChannels, blocks tiled
along the width):
    input  : numChannels rows x (numInputSamples * numBlocks) cols
             row c = [block0(F) | block1(F) | ...]
    output : numChannels rows x (numWindows * numMelBins * numBlocks) cols
             row c = [block0(W*nMel) | block1(W*nMel) | ...]
The flattened (C-order) arrays are therefore channel-major with blocks along the
row, i.e. [ch0 row][ch1 row]... exactly as the I2dToO2dDiffDims walk expects.

Reference math is logMelSpectrum.py's streaming model (identical to
AUDIOLIB/TISP_logMelSpectrum). Window + mel filterbank are built on target by
TISP::windowGen / TISP::melFbankGen; only the enum/scalar params are baked in.
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
from logMelSpectrum import TISP_logMelSpectrum, WINDOW_TYPE_ENUM, MEL_SCALE_ENUM

# Maps CSV windowAlign string to the C-side enum (0 = center-padded, 1 = left-aligned)
WINDOW_ALIGN_ENUM = {"center": 0, "left": 1}
# Maps CSV melNorm string to a TISP-side enum uint32_t value
MEL_NORM_ENUM = {"none": 0, "slaney": 1}

FLOAT_BYTES = 4

IDAT_FILE = ("../../../../superNode/i2d_to_o2d_generic/"
             "TISP_logMelSpectrum_superNode/TISP_logMelSpectrum_superNode_idat.c")
IDAT_HEADER_INCLUDE = '#include "TISP_logMelSpectrum_superNode_idat.h"'
STRUCT_NAME = "TISP_logMelSpectrum_superNode_testParams_t"


def derived_num_windows(fftSize, hopSize, numInputSamples):
    """numWindows for the streaming model (mirrors LogMelSpectrum::calcNumWindows)."""
    n, h, f = fftSize, hopSize, numInputSamples
    if f < n:
        overlapBufSize = n - h
    else:
        firstFrameWindows = (f - n) // h + 1
        overlapBufSize = f - firstFrameWindows * h
    return (overlapBufSize + f - n) // h + 1


def _parse_fmax(raw, sampleRate):
    """CSV fMax cell; empty / none / nyquist -> sampleRate/2."""
    s = "" if raw is None else str(raw).strip().lower()
    return (sampleRate / 2.0) if s in ("", "none", "nyquist", "null") else float(s)


def gen_testCaseParams(testParamsFile, currPrm):
    """Write one TISP_logMelSpectrum_superNode_testParams_t initializer to the idat file."""
    testId = currPrm["ID"]
    fftSize = currPrm["fftSize"]
    hopSize = currPrm["hopSize"]
    numInputSamples = currPrm["numInputSamples"]
    numMelBins = currPrm["numMelBins"]
    numChannels = currPrm["numChannels"]
    numBlocks = currPrm["numBlocks"]
    sampleRate = currPrm["sampleRate"]
    fMaxResolved = _parse_fmax(currPrm["fMax"], sampleRate)

    windowTypeEnum = WINDOW_TYPE_ENUM[currPrm["windowType"]]
    windowAlignEnum = WINDOW_ALIGN_ENUM[currPrm["windowAlign"]]
    windowPeriodicVal = 1 if currPrm["windowPeriodic"] else 0
    melScaleEnum = MEL_SCALE_ENUM[currPrm["melScale"]]
    melNormEnum = MEL_NORM_ENUM[currPrm["melNorm"]]

    numWindows = derived_num_windows(fftSize, hopSize, numInputSamples)
    dataPitchIn = numInputSamples * numBlocks * FLOAT_BYTES
    dataPitchOut = numWindows * numMelBins * numBlocks * FLOAT_BYTES

    if currPrm["testType"] in ["STATIC", "SINE"]:
        staticIn = "staticRefInCase" + str(testId)
        staticOut = "staticRefOutCase" + str(testId)
        testType = "STATIC"
    else:
        staticIn = "NULL"
        staticOut = "NULL"
        testType = currPrm["testType"]

    if currPrm["C7100"] == "FALSE":
        testParamsFile.write("#if !defined(__C7100__)\n")

    if currPrm["demoCase"] == "TRUE":
        testParamsFile.write(
            "#if (defined(ALL_TEST_CASES) || (TEST_CASE == %d) || defined(DEMO_CASE))\n" % int(testId))
    else:
        testParamsFile.write("#if (defined(ALL_TEST_CASES) || (TEST_CASE == %d))\n" % int(testId))

    testParamsFile.write("{\n")
    testParamsFile.write("%s, // testPattern \n" % testType)
    testParamsFile.write("(float *) %s, // *staticIn \n" % staticIn)
    testParamsFile.write("(float *) %s, // *staticOut \n" % staticOut)
    testParamsFile.write("%s, // fftSize \n" % fftSize)
    testParamsFile.write("%s, // windowLength \n" % currPrm["windowLength"])
    testParamsFile.write("%s, // hopSize \n" % hopSize)
    testParamsFile.write("%s, // numChannels \n" % numChannels)
    testParamsFile.write("%s, // numInputSamples \n" % numInputSamples)
    testParamsFile.write("%s, // numMelBins \n" % numMelBins)
    testParamsFile.write("%s, // sampleRate \n" % sampleRate)
    testParamsFile.write("%.10ef, // fMin \n" % float(currPrm["fMin"]))
    testParamsFile.write("%.10ef, // fMax \n" % float(fMaxResolved))
    testParamsFile.write("%.10ef, // power \n" % float(currPrm["power"]))
    testParamsFile.write("%.10ef, // logOffset \n" % float(currPrm["logOffset"]))
    testParamsFile.write("%d, // melScale (%s) \n" % (melScaleEnum, currPrm["melScale"]))
    testParamsFile.write("%d, // melNorm (%s) \n" % (melNormEnum, currPrm["melNorm"]))
    testParamsFile.write("%d, // windowType (%s) \n" % (windowTypeEnum, currPrm["windowType"]))
    testParamsFile.write("%d, // windowAlign (%s) \n" % (windowAlignEnum, currPrm["windowAlign"]))
    testParamsFile.write("%d, // windowPeriodic (%s) \n"
                         % (windowPeriodicVal, "periodic" if currPrm["windowPeriodic"] else "symmetric"))
    testParamsFile.write("%d, // numBlocks \n" % int(numBlocks))
    testParamsFile.write("%d, // dataPitchIn (bytes) \n" % dataPitchIn)
    testParamsFile.write("%d, // dataPitchOut (bytes) \n" % dataPitchOut)
    testParamsFile.write("%s, // outputDataLocation \n" % currPrm["outputDataLocation"])
    testParamsFile.write("%d, // numReps \n" % int(currPrm["numReps"]))
    testParamsFile.write("%d, // testID \n" % int(testId))
    testParamsFile.write("},\n")
    testParamsFile.write("#endif\n")

    if currPrm["C7100"] == "FALSE":
        testParamsFile.write("#endif // if !defined(__C7100__)\n")

    testParamsFile.write("\n")


def gen_idat_file(idatFile, testCases):
    """Generate the idat .c file."""
    with open("../../../common/ti_lice_header_idat.txt", "r") as fIn:
        idatFile.writelines(fIn.readlines())
    idatFile.write(IDAT_HEADER_INCLUDE + "\n")
    idatFile.write("/*This file is autogenerated*/\n")
    idatFile.write("/*Please do not edit*/\n\n\n")

    for testCase in testCases:
        if testCase["testType"] in ["STATIC", "SINE"]:
            TISP_utils.gen_ifdefs(idatFile, testCase)

    idatFile.write("static %s testParams[] =  {\n\n" % STRUCT_NAME)
    for testCase in testCases:
        gen_testCaseParams(idatFile, testCase)
    idatFile.write("};\n\n")

    with open("test_param_function.txt", "r") as fIn:
        idatFile.writelines(fIn.readlines())


def process_test_case(testCase):
    """Convert CSV strings to the right types."""
    testCase["fftSize"] = int(testCase["fftSize"])
    testCase["windowLength"] = int(testCase.get("windowLength", testCase["fftSize"]))
    testCase["hopSize"] = int(testCase["hopSize"])
    testCase["numChannels"] = int(testCase["numChannels"])
    testCase["numInputSamples"] = int(testCase["numInputSamples"])
    testCase["numMelBins"] = int(testCase["numMelBins"])
    testCase["sampleRate"] = int(testCase["sampleRate"])
    testCase["fMin"] = float(testCase.get("fMin", 0.0))
    testCase["fMax"] = testCase.get("fMax", None)
    testCase["power"] = float(testCase.get("power", 2.0))
    testCase["logOffset"] = float(testCase.get("logOffset", 1e-9))
    testCase["numBlocks"] = int(testCase.get("numBlocks", 1))
    testCase["numReps"] = int(testCase.get("numReps", 1))
    testCase["demoCase"] = testCase.get("demoCase", "FALSE").upper()
    testCase["testType"] = testCase.get("testType", "STATIC").upper()
    testCase["outputDataLocation"] = testCase.get("outputDataLocation", "AUDIOLIB_TEST_OUTPUT_MSMC")
    testCase["C7100"] = testCase.get("C7100", "TRUE").upper()
    testCase["melScale"] = testCase.get("melScale", "htk").strip().lower()
    testCase["melNorm"] = testCase.get("melNorm", "none").strip().lower()
    testCase["windowType"] = testCase.get("windowType", "hann").strip().lower()
    testCase["windowAlign"] = testCase.get("windowAlign", "center").strip().lower()
    testCase["windowPeriodic"] = str(testCase.get("windowPeriodic", "TRUE")).strip().upper() == "TRUE"
    return testCase


def gen_test_case_header_file(testCase):
    """Generate the per-case input/output header in the SuperNode 2D DMA layout.

    input  : numChannels rows x (F*numBlocks) cols, row c = concatenated blocks
    output : numChannels rows x (W*numMelBins*numBlocks) cols
    Each channel row is run through the single-channel streaming reference, which
    (processing the full concatenated signal from zero state) equals the streaming
    block-by-block result the node produces.
    """
    testId = int(testCase["ID"])
    fftSize = testCase["fftSize"]
    hopSize = testCase["hopSize"]
    numChannels = testCase["numChannels"]
    numInputSamples = testCase["numInputSamples"]
    numMelBins = testCase["numMelBins"]
    numBlocks = testCase["numBlocks"]
    sampleRate = testCase["sampleRate"]
    fMaxResolved = _parse_fmax(testCase["fMax"], sampleRate)

    np.random.seed(42 + testId)

    numWindows = derived_num_windows(fftSize, hopSize, numInputSamples)
    rowInLen = numInputSamples * numBlocks
    rowOutLen = numWindows * numMelBins * numBlocks

    # single-channel streaming reference; each channel row is independent
    proc = TISP_logMelSpectrum(
        fftSize=fftSize, windowLength=testCase["windowLength"], hopSize=hopSize,
        numChannels=1, numInputSamples=numInputSamples, numMelBins=numMelBins,
        sampleRate=sampleRate, fMin=testCase["fMin"], fMax=fMaxResolved,
        power=testCase["power"], logOffset=testCase["logOffset"],
        melScale=testCase["melScale"],
        norm=(None if testCase["melNorm"] == "none" else testCase["melNorm"]),
        numBlocks=numBlocks, windowType=testCase["windowType"],
        align=testCase["windowAlign"], periodic=testCase["windowPeriodic"],
    )

    # channel-major rows; blocks tiled along the row width
    x = np.random.uniform(-1.0, 1.0, (numChannels, rowInLen)).astype(np.float32)
    y = np.zeros((numChannels, rowOutLen), dtype=np.float32)
    for chIdx in range(numChannels):
        # for one channel, the row [b0(F) b1(F) ...] IS the block-major exec() input
        y[chIdx, :] = proc.exec(x[chIdx, :]).astype(np.float32)

    headerFileName = "staticRefCase" + str(testId) + ".h"
    # flatten C-order -> [ch0 row][ch1 row]... (matches the I2dToO2dDiffDims walk)
    file_io.write_header_file(x.reshape(-1), y.reshape(-1), testCase, headerFileName)

    print("Test case %d: fftSize=%d hop=%d numCh=%d F=%d nMels=%d numWindows=%d power=%g numBlocks=%d "
          "=> input=%d output=%d floats"
          % (testId, fftSize, hopSize, numChannels, numInputSamples, numMelBins, numWindows,
             testCase["power"], numBlocks, x.size, y.size))


parser = argparse.ArgumentParser()
parser.add_argument("-a", "--allCases", action="store_true", help="Generate all test cases")
myArgs = parser.parse_args()

testCasesCsvFile = "test_cases_list.csv"
testIdList = list(range(0, 20))


def gen_test_case(testCase):
    if testCase["testType"] in ["STATIC", "SINE"]:
        gen_test_case_header_file(testCase)
    else:
        print("Skipping test case generation for test ID", testCase["ID"], "as testType is RANDOM")


with open(testCasesCsvFile, encoding="utf-8-sig") as csv_file:
    testCases = [process_test_case(tc) for tc in csv.DictReader(csv_file)]

    for testCase in testCases:
        if myArgs.allCases or int(testCase["ID"]) in testIdList:
            gen_test_case(testCase)


with open(IDAT_FILE, "w") as idatFile:
    gen_idat_file(idatFile, testCases)


if platform in ("linux", "linux2"):
    import shutil
    if shutil.which("clang-format"):
        call(["clang-format", "-style=file", "-i", IDAT_FILE])
    if shutil.which("indent"):
        call(["indent", "-nut", "-i3", "-c55", "-l200", IDAT_FILE])
        call(["rm", "-f", IDAT_FILE + "~"])


print("\nIdat file generation completed")
