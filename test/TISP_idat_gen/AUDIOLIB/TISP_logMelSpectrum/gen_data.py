#!/usr/bin/env python3
"""
logMelSpectrum test data generator for TISP library.

Generates input (waveform) and expected output (log-mel spectrogram) for the
AUDIOLIB logMelSpectrum feature-extraction kernel used by the audioai-vggish and
yamnet-audioset front-ends.

The pipeline is STFT -> |.|^power spectrogram -> mel filterbank matmul ->
log(mel + logOffset).  The reference is computed by ``logMelSpectrum.py`` (torch
STFT + torchaudio mel filterbank).  Window coefficients and the mel filterbank
are re-derived on the target from the scalar parameters (sampleRate, fMin, fMax,
numMelBins, melScale, windowType, ...) exactly as ``TISP::windowGen`` handles the
window for STFT — only the enum/scalar parameters are baked into the test data,
not the coefficient tables.

The kernel is a *streaming* STFT-based log-mel (zero-initialised overlap state,
same model as the TISP Stft node), so all test vectors are generated with that
streaming model and the reference matches the node bit-for-bit.  Reflect-pad
"center" alignment is a whole-clip concept handled off-device; the host-only
``torch_log_mel`` model reference and the streaming-vs-model gap report live in
``logMelSpectrum.py``'s __main__ self-check.
"""

from __future__ import print_function
from subprocess import call
from sys import platform

import sys
import numpy as np
import csv
import torch
import torchaudio
import scipy.signal
import argparse

sys.path.append("../../common")
import TISP_utils
import file_io

from logMelSpectrum import (
    TISP_logMelSpectrum,
    WINDOW_TYPE_ENUM,
    MEL_SCALE_ENUM,
)

# Maps CSV windowAlign string to the C-side enum (0=center-padded, 1=left-aligned)
WINDOW_ALIGN_ENUM = {"center": 0, "left": 1}

# Maps CSV melNorm string to a TISP-side enum uint32_t value
MEL_NORM_ENUM = {"none": 0, "slaney": 1}


def gen_testCaseParams(testParamsFile, currPrm):
    """Generate and write test case parameters to idat file."""
    testId = currPrm["ID"]
    fftSize = currPrm["fftSize"]
    windowLength = currPrm["windowLength"]
    hopSize = currPrm["hopSize"]
    numChannels = currPrm["numChannels"]
    numInputSamples = currPrm["numInputSamples"]
    numMelBins = currPrm["numMelBins"]
    sampleRate = currPrm["sampleRate"]
    fMin = currPrm["fMin"]
    fMax = currPrm["fMax"]
    power = currPrm["power"]
    logOffset = currPrm["logOffset"]
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
    melScaleStr = currPrm.get("melScale", "htk")
    melScaleEnum = MEL_SCALE_ENUM[melScaleStr]
    melNormStr = currPrm.get("melNorm", "none")
    melNormEnum = MEL_NORM_ENUM[melNormStr]

    # torchaudio resolves f_max=None to sampleRate/2; bake the resolved value.
    fMaxResolved = fMax if fMax is not None else sampleRate / 2.0

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
    testParamsFile.write("%s, // numMelBins \n" % numMelBins)
    testParamsFile.write("%s, // sampleRate \n" % sampleRate)
    testParamsFile.write("%.10ef, // fMin \n" % float(fMin))
    testParamsFile.write("%.10ef, // fMax \n" % float(fMaxResolved))
    testParamsFile.write("%.10ef, // power \n" % float(power))
    testParamsFile.write("%.10ef, // logOffset \n" % float(logOffset))
    testParamsFile.write("%d, // melScale (%s) \n" % (melScaleEnum, melScaleStr))
    testParamsFile.write("%d, // melNorm (%s) \n" % (melNormEnum, melNormStr))
    testParamsFile.write("%d, // windowType (%s) \n" % (windowTypeEnum, windowTypeStr))
    testParamsFile.write("%d, // windowAlign (%s) \n" % (windowAlignEnum, windowAlignStr))
    testParamsFile.write(
        "%d, // windowPeriodic (%s) \n"
        % (windowPeriodicVal, "periodic" if windowPeriodic else "symmetric")
    )
    testParamsFile.write("%s, // outputDataLocation \n" % outputDataLocation)
    testParamsFile.write("%d, // numReps \n" % int(numReps))
    testParamsFile.write("%d, // testID \n" % int(testId))
    testParamsFile.write("%d, // numBlocks \n" % int(numBlocks))
    testParamsFile.write("},\n")
    testParamsFile.write("#endif\n")

    if C7100 == "FALSE":
        testParamsFile.write("#endif // if !defined(__C7100__)\n")

    testParamsFile.write("\n")


def gen_idat_file(idatFile, testCases):
    """Generate the idat file."""

    fIn = open("../../common/ti_lice_header_idat.txt", "r")
    lines = fIn.readlines()
    fIn.close()
    idatFile.writelines(lines)
    includeString = '#include "TISP_logMelSpectrum_idat.h"'
    idatFile.writelines(includeString)
    idatFile.write("\n")
    idatFile.write("/*This file is autogenerated*/\n")
    idatFile.write("/*Please do not edit*/\n")
    idatFile.write("\n\n")

    # generate ifdefs for header files
    for testCase in testCases:
        if testCase["testType"] in ["STATIC", "SINE"]:
            TISP_utils.gen_ifdefs(idatFile, testCase)

    # generate parameters for test cases
    idatFile.write("static TISP_logMelSpectrum_testParams_t testParams[] =  ")
    idatFile.write("{\n\n")

    for testCase in testCases:
        gen_testCaseParams(idatFile, testCase)

    idatFile.write("};\n\n")

    fIn = open("test_param_function.txt", "r")
    lines = fIn.readlines()

    idatFile.writelines(lines)


def _parse_fmax(raw):
    """Parse the CSV fMax cell; empty / none / nyquist -> None (=> sampleRate/2)."""
    if raw is None:
        return None
    s = str(raw).strip().lower()
    if s in ("", "none", "nyquist", "null"):
        return None
    return float(s)


def process_test_case(testCase):
    """Convert str to typed values for necessary parameters."""
    testCase["testId"] = int(testCase["ID"])
    testCase["fftSize"] = int(testCase["fftSize"])
    testCase["windowLength"] = int(testCase.get("windowLength", testCase["fftSize"]))
    testCase["hopSize"] = int(testCase["hopSize"])
    testCase["numChannels"] = int(testCase["numChannels"])
    testCase["numInputSamples"] = int(testCase["numInputSamples"])
    testCase["numMelBins"] = int(testCase["numMelBins"])
    testCase["sampleRate"] = int(testCase["sampleRate"])
    testCase["fMin"] = float(testCase.get("fMin", 0.0))
    testCase["fMax"] = _parse_fmax(testCase.get("fMax", None))
    testCase["power"] = float(testCase.get("power", 2.0))
    testCase["logOffset"] = float(testCase.get("logOffset", 1e-9))
    testCase["numBlocks"] = int(testCase.get("numBlocks", 1))
    testCase["numReps"] = int(testCase.get("numReps", 1))
    testCase["demoCase"] = testCase.get("demoCase", "FALSE").upper()
    testCase["testType"] = testCase.get("testType", "STATIC").upper()
    testCase["outputDataLocation"] = testCase.get(
        "outputDataLocation", "AUDIOLIB_TEST_OUTPUT_MSMC"
    )
    testCase["C7100"] = testCase.get("C7100", "TRUE").upper()
    testCase["dType"] = testCase.get("dType", "float")
    testCase["melScale"] = testCase.get("melScale", "htk").strip().lower()
    testCase["melNorm"] = testCase.get("melNorm", "none").strip().lower()
    testCase["windowType"] = testCase.get("windowType", "hann").strip().lower()
    testCase["windowAlign"] = testCase.get("windowAlign", "center").strip().lower()
    testCase["windowPeriodic"] = (
        str(testCase.get("windowPeriodic", "TRUE")).strip().upper() == "TRUE"
    )

    return testCase


def gen_test_case_header_file(testCase):
    """Generate test data for a single test case and write header file.

    For numBlocks > 1, generates multi-block input and reference output.
    Input layout (block-major):  [block0(F*M) | block1(F*M) | ...]
    Output layout (block-major): [block0(W*nMels*M) | block1(W*nMels*M) | ...]

    The reference is computed with logMelSpectrum.log_mel_reference on the full
    per-channel signal (zeros(overlapBufSize) + all blocks concatenated), which is
    mathematically equivalent to streaming block-by-block with state carry-over.
    """
    testId = int(testCase["ID"])
    fftSize = testCase["fftSize"]
    windowLength = testCase["windowLength"]
    hopSize = testCase["hopSize"]
    numChannels = testCase["numChannels"]
    numInputSamples = testCase["numInputSamples"]
    numMelBins = testCase["numMelBins"]
    sampleRate = testCase["sampleRate"]
    fMin = testCase["fMin"]
    fMax = testCase["fMax"]
    power = testCase["power"]
    logOffset = testCase["logOffset"]
    melScale = testCase["melScale"]
    melNorm = testCase["melNorm"]
    numBlocks = testCase["numBlocks"]
    windowTypeStr = testCase["windowType"]
    windowAlignStr = testCase["windowAlign"]
    windowPeriodic = testCase["windowPeriodic"]

    np.random.seed(42 + testId)

    # Build the processor; it owns derived geometry (numWindows, overlapBufSize)
    # and the mel filterbank, matching the C kernel's parameter-driven setup.
    proc = TISP_logMelSpectrum(
        fftSize=fftSize,
        windowLength=windowLength,
        hopSize=hopSize,
        numChannels=numChannels,
        numInputSamples=numInputSamples,
        numMelBins=numMelBins,
        sampleRate=sampleRate,
        fMin=fMin,
        fMax=fMax,
        power=power,
        logOffset=logOffset,
        melScale=melScale,
        norm=(None if melNorm == "none" else melNorm),
        numBlocks=numBlocks,
        windowType=windowTypeStr,
        align=windowAlignStr,
        periodic=windowPeriodic,
    )

    # random input: numBlocks * F * M floats (block-major, channel-contiguous)
    inputLen = numBlocks * numInputSamples * numChannels
    x = np.random.uniform(-1.0, 1.0, inputLen).astype(np.float32)

    # expected output: numBlocks * numWindows * numMelBins * numChannels floats
    y = proc.exec(x).astype(np.float32)

    headerFileName = "staticRefCase" + str(testId) + ".h"
    file_io.write_header_file(x, y, testCase, headerFileName)

    print(
        "Test case %d: fftSize=%d winLen=%d hop=%d numCh=%d F=%d nMels=%d "
        "numWindows=%d sr=%d power=%g numBlocks=%d win=%s mel=%s "
        "=> input=%d output=%d floats"
        % (
            testId,
            fftSize,
            windowLength,
            hopSize,
            numChannels,
            numInputSamples,
            numMelBins,
            proc.numWindows,
            sampleRate,
            power,
            numBlocks,
            windowTypeStr,
            melScale,
            len(x),
            len(y),
        )
    )


# Parse command line arguments
parser = argparse.ArgumentParser()
parser.add_argument("-a", "--allCases", action="store_true", help="Generate all test cases")
myArgs = parser.parse_args()
# CSV file with parameters for all test cases
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


#############################################################################
# Open CSV file with test cases and generate header file with test data for #
# test cases of interest; based on testIdList                               #
#############################################################################

with open(testCasesCsvFile, encoding="utf-8-sig") as csv_file:
    testCaseReader = csv.DictReader(csv_file)
    testCases = list(testCaseReader)

    for testCase in testCases:
        testCase = process_test_case(testCase)

    for testCase in testCases:
        if myArgs.allCases == True:
            gen_test_case(testCase)
        elif int(testCase["ID"]) in testIdList:
            gen_test_case(testCase)


idatFileName = "../../../AUDIOLIB/TISP_logMelSpectrum/TISP_logMelSpectrum_idat.c"

##########################################################################
# Generate idat.c file by parsing all the test cases in the CSV file     #
##########################################################################

with open(idatFileName, "w") as idatFile:
    gen_idat_file(idatFile, testCases)


if platform == "linux" or platform == "linux2":
    # Optional cosmetic formatting; skip silently if the tools are absent so the
    # generated data (already written above) is not lost to a missing-tool crash.
    import shutil

    if shutil.which("clang-format"):
        call(["clang-format", "-style=file", "-i", idatFileName])
    if shutil.which("indent"):
        call(["indent", "-nut", "-i3", "-c55", "-l200", idatFileName])
        call(["rm", "-f", idatFileName + "~"])


print("\nIdat file generation completed")
