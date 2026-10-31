from __future__ import print_function
from subprocess import call
from sys import platform

import sys
import numpy as np
import csv

sys.path.append("../../../common")
import TISP_utils
import file_io


class AUDIOLIB_gainSmoothing:
    """Applies gain ramp across audio frames with strict dtype enforcement."""
    
    def __init__(self, dType="float", inSamples=1, inChannels=1, currentGain=1.0, 
                 targetGain=1.0, samplingRate=48000.0, fadeTime=0.0, isInterleaved=False, 
                 fadeType="SYMMETRIC", numExecReps=1):
        
        self._dType = dType.lower()
        self._npDtype = np.float64 if self._dType == "double" else np.float32
        
        self._inSamples = int(inSamples)
        self._inChannels = int(inChannels)
        self._currentGain = float(currentGain)
        self._targetGain = float(targetGain)
        self._samplingRate = float(samplingRate)
        self._fadeTime = float(fadeTime)
        self._isInterleaved = bool(isInterleaved)
        self._fadeType = fadeType.upper()
        self._numExecReps = int(numExecReps)
        
        self._frameSize = self._inSamples  
        self._samplesPerFrame = self._inSamples * self._inChannels
        self._totalSamplesPerChannel = self._inSamples * self._numExecReps

    def _compute_symmetric_gain(self) -> np.ndarray:
        dtype = self._npDtype
        targetGain = dtype(self._targetGain)
        currentGain = dtype(self._currentGain)
        total_samples = self._totalSamplesPerChannel

        if currentGain <= dtype(0.0) or targetGain <= dtype(0.0):
            raise ValueError("Symmetric fading requires positive gains.")

        timeConstantSec = dtype(self._fadeTime / 1000.0)
        totalFadeSamples = dtype(timeConstantSec * dtype(self._samplingRate))

        pCurrentGain = np.full(total_samples, targetGain, dtype=dtype)

        if totalFadeSamples > dtype(0.0) and self._fadeTime > 0:
            fade_len = min(int(totalFadeSamples), total_samples)

            gainRatio = dtype(targetGain / currentGain)
            invSamples = dtype(dtype(1.0) / totalFadeSamples)
            gainStep = dtype(np.power(gainRatio, invSamples))

            gainCurve = np.empty(fade_len, dtype=dtype)
            accumGain = currentGain

            for s in range(fade_len):
                accumGain = dtype(accumGain * gainStep)
                gainCurve[s] = accumGain

            if gainStep > dtype(1.0):
                gainCurve = np.minimum(gainCurve, targetGain)
            else:
                gainCurve = np.maximum(gainCurve, targetGain)

            pCurrentGain[:fade_len] = gainCurve
            pCurrentGain[fade_len:] = targetGain

            
        return pCurrentGain

    def _compute_smooth_gain(self) -> np.ndarray:
        targetGain = self._npDtype(self._targetGain)
        total_samples = self._totalSamplesPerChannel
        pCurrentGain = np.zeros(total_samples, dtype=self._npDtype)
        gainThreshold = self._npDtype(1e-6)

        timeConstantSec = self._fadeTime / 1000.0
        
        if self._fadeTime <= 0.0:
            pCurrentGain[:] = targetGain
            return pCurrentGain

        alpha = self._npDtype(np.exp(-1.0 / (self._samplingRate * timeConstantSec)))
        one_minus_alpha = self._npDtype(1.0) - alpha
        
        currentGain = self._npDtype(self._currentGain)
        for frame in range(self._numExecReps):
            startIdx = frame * self._frameSize
            endIdx = startIdx + self._frameSize

            if frame > 0 and abs(pCurrentGain[startIdx - 1] - targetGain) < gainThreshold:
                pCurrentGain[startIdx:endIdx] = targetGain
                currentGain = targetGain
            else:
                for j in range(startIdx, endIdx):
                    currentGain = alpha * currentGain + one_minus_alpha * targetGain
                    pCurrentGain[j] = currentGain

        return pCurrentGain

    def exec(self, pInLocal: np.ndarray):
        pInLocal = np.asarray(pInLocal, dtype=self._npDtype)
        
        if self._fadeType == "SYMMETRIC":
            pCurrentGain = self._compute_symmetric_gain()
        elif self._fadeType == "SMOOTH":
            pCurrentGain = self._compute_smooth_gain()
        else:
            pCurrentGain = np.full(self._totalSamplesPerChannel, self._targetGain, dtype=self._npDtype)

        gain_per_frame = pCurrentGain.reshape(self._numExecReps, self._inSamples)

        if self._isInterleaved:
            gain_expanded = np.repeat(gain_per_frame, self._inChannels, axis=1)
        else:
            gain_expanded = np.tile(gain_per_frame, (1, self._inChannels))

        pInLocal_2d = pInLocal.reshape(self._numExecReps, self._samplesPerFrame)
        pOutLocal = pInLocal_2d * gain_expanded

        # Return NumPy arrays directly (flattened) to preserve exact float32 dtype
        return pOutLocal, pCurrentGain


def get_soc_guard(am275_soc_value):
    """Generate SOC preprocessor guard based on 'AM275 SOC' column value"""
    if am275_soc_value.lower() == "yes":
        return None
    else:
        return "!(SOC == AM275 && !defined(HOST_EMULATION))"


def gen_testCaseParams(testParamsFile, currPrm):
    """Generate and write test case parameters to idat file

    Args:
        param1 (file object): Input file object to idat.c file
        param2 (dict object): Dict object with test case parameter

    """
    dType = currPrm["dType"]
    widthIn = currPrm["widthIn"]
    heightIn = currPrm["heightIn"]
    blockWidth = currPrm["blockWidth"]
    blockHeight = currPrm["blockHeight"]
    isInterleave = currPrm["isInterleave"]
    inPitch = currPrm["inPitch"]
    outPitch = currPrm["outPitch"]
    alignStride = currPrm["alignStride"]
    testPattern = currPrm["testPattern"]

    testId = currPrm["ID"]

    # Map fadeType string to integer
    fadeType_map = {"SYMMETRIC": 0, "SMOOTH": 1}
    am275_soc = currPrm.get("AM275 SOC", "yes")

    in0 = "staticRefInCase" + testId
    out = "staticRefOutCase" + testId

    testParamsFile.write(
        "#if (defined(ALL_TEST_CASES) || (TEST_CASE == %d))\n" % (int(testId))
    )

    soc_guard = get_soc_guard(am275_soc)
    if soc_guard:
        idatFile.write("#if %s\n" % soc_guard)

    idatFile.write("{\n")
    idatFile.write("%s, // testPattern \n" % ((testPattern)))
    idatFile.write("%s, // *staticIn \n" % ((in0)))
    idatFile.write("%s, // *staticOut\n" % ((out)))
    idatFile.write("%s, // dataType \n" % ((TISP_utils.resolve_AUDIOLIB_dType(dType))))
    idatFile.write("%s, // widthIn \n" % ((widthIn)))
    idatFile.write("%s, // heightIn \n" % ((heightIn)))
    idatFile.write("%s, // blockWidth \n" % ((blockWidth)))
    idatFile.write("%s, // blockHeight \n" % ((blockHeight)))
    idatFile.write("%s, // isInterleave \n" % ((isInterleave)))
    idatFile.write("%sf, // samplingRate \n" % ((currPrm["samplingRate"])))
    idatFile.write("%sf, // fadeTime \n" % ((currPrm["fadeTime"])))
    idatFile.write("%d, // fadeType (0=SYMMETRIC, 1=SMOOTH) \n" % (fadeType_map[currPrm["fadeType"]]))
    idatFile.write("%sf, // currentGain \n" % ((currPrm["currentGain"])))
    idatFile.write("%sf, // targetGain \n" % ((currPrm["targetGain"])))
    if alignStride == "FALSE":
        # idatFile.write("%s, // strideIn0 \n" % ((strideIn0)))
        idatFile.write("%s * sizeof(%s), // strideIn0 \n" % ((inPitch, dType)))
        idatFile.write("%s * sizeof(%s), // strideOut \n" % ((outPitch, dType)))
    else:
        idatFile.write(
            "AUDIOLIB_CALC_STRIDE(%s * sizeof(%s), AUDIOLIB_ALIGN_SHIFT_64BYTES), // strideIn0 \n"
            % ((inPitch), (dType))
        )
        idatFile.write(
            "AUDIOLIB_CALC_STRIDE(%s * sizeof(%s), AUDIOLIB_ALIGN_SHIFT_64BYTES), // strideOut \n"
            % ((outPitch), (dType))
        )
    idatFile.write("%d // test ID\n" % int(testId))
    idatFile.write("},\n")
    if soc_guard:
        idatFile.write("#endif  // SOC guard\n")
    idatFile.write("#endif  // TEST_CASE guard\n")

    idatFile.write("\n\n")


def gen_idat_file(idatFile, testCases):
    """Generate the idat file

    Args:
        param1 (file object): Input file object to idat.c file
        param2 (dict object): Dict object with test case parameter

    """

    fIn = open("../../../common/ti_lice_header_idat.txt", "r")
    lines = fIn.readlines()
    fIn.close()
    idatFile.writelines(lines)
    includeString = '#include "TISP_gainSmoothing_superNode_idat.h"'
    idatFile.writelines(includeString)
    idatFile.write("\n")
    idatFile.write("/*This file is autogenerated*/\n")
    idatFile.write("/*Please do not edit*/\n")
    idatFile.write("\n\n")
    # generate ifdefs for header files
    for testCase in testCases:
        TISP_utils.gen_ifdefs(idatFile, testCase)

    # generate parameters for test cases
    idatFile.write("static TISP_gainSmoothing_superNode_testParams_t testParams[] =  ")
    idatFile.write("{\n")

    for testCase in testCases:
        gen_testCaseParams(idatFile, testCase)

    idatFile.write("};\n\n")
    fIn = open("test_param_function.txt", "r")
    lines = fIn.readlines()

    idatFile.writelines(lines)


def process_test_case(testCase):
    """Convert str to int for necessary parameters

    Args:
        Param1(:obj: dict) : Dictionary object with test case parameters

    Returns:
        Obj: Dictionary object with necessary parameters converted to int

    """

    # print(testCase.keys())
    testCase["testId"] = int(testCase["ID"])
    testCase["widthIn"] = int(testCase["widthIn"])
    testCase["heightIn"] = int(testCase["heightIn"])
    testCase["blockWidth"] = int(testCase["blockWidth"])
    testCase["blockHeight"] = int(testCase["blockHeight"])
    testCase["isInterleave"] = int(testCase["isInterleave"])
    testCase["inPitch"] = int(testCase["inPitch"])
    testCase["outPitch"] = int(testCase["outPitch"])
    testCase["samplingRate"] = float(testCase["samplingRate"])
    testCase["fadeTime"] = float(testCase["fadeTime"])
    testCase["fadeType"] = testCase["fadeType"].upper()
    testCase["currentGain"] = float(testCase["currentGain"])
    testCase["targetGain"] = float(testCase["targetGain"])

    return testCase


def gen_test_case_header_file(testCase):
    """Generate test case and write out input, filter, and output tensor
    into a header file usable in C/C++ code.

    Args:
        param1 (:obj: dict): A dict object with all the test case
        parameters

    Returns:
        [int int]: Minimum and maximum value in output feature tensor

    """

    ##############################
    # Get test parameter of case #
    ##############################
    testId = testCase["ID"]
    dType = testCase["dType"]
    widthIn = testCase["widthIn"]
    heightIn = testCase["heightIn"]
    isInterleave = testCase["isInterleave"]

    # [minVal, maxVal] = AUDIOLIB_utils.minMaxVal(dType)
    [minVal, maxVal] = [-10, 10]

    if dType[0] == "f":
        in0 = np.random.uniform(
            low=minVal, high=maxVal, size=(heightIn, widthIn)
        ).astype("float32")

    if isInterleave:
        in0 = in0.T

    # The node starts at rest on currentGain; the target gain set after construction starts the ramp
    model = AUDIOLIB_gainSmoothing(dType=dType, inSamples=widthIn, inChannels=heightIn,
                                   currentGain=testCase["currentGain"], targetGain=testCase["targetGain"],
                                   samplingRate=testCase["samplingRate"], fadeTime=testCase["fadeTime"],
                                   isInterleaved=isInterleave, fadeType=testCase["fadeType"], numExecReps=1)
    outBlocks, _ = model.exec(in0[np.newaxis])
    out = outBlocks[0].reshape(in0.shape).astype("float32")

    headerFileName = "staticRefCase" + str(testId) + ".h"
    file_io.write_header_file(in0, out, testCase, headerFileName)

    return [np.min(out), np.max(out)]


# CSV file with paratmeters for all test cases
testCasesCsvFile = "test_cases_list.csv"
testIdList = []  # list of test IDs that we want to generate test data
testIdList += range(0, 100)
# testIdList = [18]

myArgs = TISP_utils.getCmdLineArgs()


def gen_test_case(testCase):
    [minY, maxY] = gen_test_case_header_file(testCase)
    print(
        "Test case generation completed for test ID",
        testCase["ID"],
        "w/ min:",
        minY,
        "and max",
        maxY,
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


idatFileName = "../../../../superNode/i2d_to_o2d/TISP_gainSmoothing_superNode/TISP_gainSmoothing_superNode_idat.cpp"


##########################################################################
# Generate idat.cpp file by parsing all the test cases in the CSV file # #
##########################################################################


with open(idatFileName, "w") as idatFile:
    gen_idat_file(idatFile, testCases)


if platform == "linux" or platform == "linux2":
    call(["clang-format", "-style=file", "-i", idatFileName])
    call(["indent", "-nut", "-i3", "-c55", "-l200", idatFileName])
    call(["rm", idatFileName + "~"])  # remove backup file


print("Idat file generation completed")
