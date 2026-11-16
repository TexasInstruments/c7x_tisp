# Copyright (C) 2026 Texas Instruments Incorporated
#
# SPDX-License-Identifier: LicenseRef-TI-TFL

from __future__ import print_function
from subprocess import call
from sys import platform

import sys
import numpy as np
import csv

sys.path.append("../../common")
import TISP_utils
import file_io

from cascadeBiquad import CascadeBiquad


def gen_testCaseParams(testParamsFile, currPrm):
    """Generate and write test case parameters to idat file

    Args:
        param1 (file object): Input file object to idat.c file
        param2 (dict object): Dict object with test case parameter

    """

    testId = currPrm["ID"]
    testType = currPrm["testType"]
    dataType = currPrm["dataType"]
    biquadForm = currPrm["biquadForm"]
    dataSize = currPrm["dataSize"]
    numChannels = currPrm["numChannels"]
    numStages = currPrm["numStages"]
    dataPitch = currPrm["dataPitch"]
    filterVarPitch = currPrm["filterVarPitch"]
    enableNChCoeffs = currPrm["enableNChCoeffs"]
    enableCalcCoeffs = currPrm["enableCalcCoeffs"]
    numExecReps = currPrm["numExecReps"]
    numReps = currPrm["numReps"]
    demoCase = currPrm["demoCase"]
    outputDataLocation = currPrm["outputDataLocation"]
    testCategory = currPrm["testCategory"]
    testAm275Evm = currPrm["testAm275Evm"]

    if testType == "STATIC":
        inputData = "staticRefInCase" + testId
        filterCoefData = "staticRefFilterCoefCase" + testId
        outputData = "staticRefOutCase" + testId
        if (biquadForm == "DSPLIB_BIQUAD_TIF") or (
            (enableNChCoeffs == "True")
            and (
                biquadForm
                in ["DSPLIB_BIQUAD_DF1", "DSPLIB_BIQUAD_DF2", "DSPLIB_BIQUAD_TDF2"]
            )
        ):
            optFilterCoefData = "staticRefFilterCoefOptCase" + testId
        else:
            optFilterCoefData = "NULL"
    else:
        inputData = "NULL"
        filterCoefData = "NULL"
        outputData = "NULL"
        optFilterCoefData = "NULL"

    if demoCase == "True":
        testParamsFile.write(
            "#if (defined(ALL_TEST_CASES) || (TEST_CASE == %d) || defined(DEMO_CASE))\n"
            % (int(testId))
        )
    elif testAm275Evm == "False":
        testParamsFile.write(
            # Same tests execute in Host Emulation and AM275 EVM
            # "if (defined(ALL_TEST_CASES) || (TEST_CASE == %d) || (TEST_CATEGORY == %d) && (SOC != AM275))\n" % (int(testId), int(testCategory))
            #
            # Test only excluded on AM275 EVM
            "#if (!defined(EVM_TEST) && (defined(ALL_TEST_CASES) || (TEST_CASE == %d) || (TEST_CATEGORY == %d))) ||"
            "(defined(EVM_TEST) && (SOC != AM275) && (defined(ALL_TEST_CASES) || (TEST_CASE == %d) || (TEST_CATEGORY == %d)))\n"
            % (int(testId), int(testCategory), int(testId), int(testCategory))
        )
    else:
        testParamsFile.write(
            "#if (defined(ALL_TEST_CASES) || (TEST_CASE == %d) || (TEST_CATEGORY == %d))\n"
            % (int(testId), int(testCategory))
        )

    DSPLIB_dataType = TISP_utils.resolve_DSPLIB_dType(dataType)

    idatFile.write("{\n")
    idatFile.write("%s, // Test pattern \n" % ((testType)))
    idatFile.write("%s, // Input vector \n" % ((inputData)))
    idatFile.write("%s, // Filter coefficient vector \n" % ((filterCoefData)))
    idatFile.write("%s, // Output vector \n" % ((outputData)))
    idatFile.write(
        "%s, // Optimized filter coefficient vector \n" % ((optFilterCoefData))
    )
    idatFile.write("%s, // dataType \n" % ((DSPLIB_dataType)))
    idatFile.write("%d, // dataSize \n" % ((dataSize)))
    idatFile.write("%d, // numChannels \n" % ((numChannels)))
    idatFile.write("%d, // numStages \n" % ((numStages)))
    idatFile.write("%s, // biquadForm \n" % ((biquadForm)))
    idatFile.write("%d, // dataPitch \n" % ((dataPitch)))
    idatFile.write("%d, // filterVarPitch \n" % ((filterVarPitch)))
    idatFile.write(
        "%d, // enableNChCoeffs \n" % (1 if enableNChCoeffs == "True" else 0)
    )
    idatFile.write(
        "%d, // enableCalcCoeffs \n" % (1 if enableCalcCoeffs == "True" else 0)
    )
    idatFile.write("%d, // bypassMode \n" % (currPrm["bypassMode"]))
    idatFile.write("%s, // Output data location \n" % (outputDataLocation))
    # idatFile.write("%d, // Number of execution repetitions \n" % (int(numExecReps)))
    idatFile.write("%d, // Number of repetitions \n" % (int(numReps)))
    idatFile.write("%d, // test ID\n" % int(testId))
    idatFile.write("},\n")
    idatFile.write("#endif")
    idatFile.write("\n\n")


def gen_idat_file(idatFile, testCases):
    """Generate the idat file

    Args:
        param1 (file object): Input file object to idat.c file
        param2 (dict object): Dict object with test case parameter

    """

    fIn = open("../../common/ti_lice_header_idat.txt", "r")
    lines = fIn.readlines()
    fIn.close()
    idatFile.writelines(lines)
    includeString = '#include "TISP_cascadeBiquad_idat.hpp"'
    idatFile.writelines(includeString)
    idatFile.write("\n")
    idatFile.write("/*This file is autogenerated*/\n")
    idatFile.write("/*Please do not edit*/\n")
    idatFile.write("\n\n")
    # generate ifdefs for header files
    for testCase in testCases:
        if testCase["testType"] == "STATIC":
            TISP_utils.gen_ifdefs(idatFile, testCase)

    # generate parameters for test cases
    idatFile.write("static DSPLIB_cascadeBiquad_testParams_t testParams[] =  ")
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
    testCase["dataSize"] = int(testCase["dataSize"])
    testCase["numChannels"] = int(testCase["numChannels"])
    testCase["numStages"] = int(testCase["numStages"])
    testCase["dataPitch"] = int(testCase["dataPitch"])
    testCase["filterVarPitch"] = int(testCase["filterVarPitch"])
    testCase["numExecReps"] = int(testCase["numExecReps"])
    testCase["numReps"] = int(testCase["numReps"])
    testCase["testCategory"] = int(testCase["testCategory"])
    testCase["bypassMode"] = int(testCase["bypassMode"])

    return testCase


def gen_test_case_header_file(testCase):
    """Generate test case data and write out input, filter, and output tensor
    into a header file usable in C/C++ code.

    Args:
        param1 (:obj: dict): A dict object with all the test case
        parameters

    Returns:
        [int int]: Minimum and maximum value in output feature tensor

    """

    testId = testCase["ID"]

    numExecReps = testCase["numExecReps"]
    dataSize = testCase["dataSize"]

    # SDK test signal and filter coefficients (the coefficients are a constructor argument)
    (
        inputData,
        pCoeffs,
        sosCoefs,
        zi,
        tifBiquadCascadeCoefs,
        tifBiquadCascadeGains,
    ) = CascadeBiquad.generate_inputs(
        dataType=testCase["dataType"],
        dataSize=dataSize,
        numChannels=testCase["numChannels"],
        numStages=testCase["numStages"],
        enableNChCoeffs=testCase["enableNChCoeffs"],
        biquadForm=testCase["biquadForm"],
        numExecReps=numExecReps,
    )

    # Build the model with the case's values (the reference runs in the precision the
    # test signal is generated in: "float" is generated, and filtered, as float64)
    kernel = CascadeBiquad(
        num_channels=testCase["numChannels"],
        num_samples=dataSize,
        in_dtype=inputData.dtype.name,
        numStages=testCase["numStages"],
        biquadForm=testCase["biquadForm"],
        pCoeffs=pCoeffs,
        bypassMode=testCase["bypassMode"],
    )

    # Run frame by frame; the filter state stays in the model (bypass is handled by exec)
    outputData = np.zeros_like(inputData)
    for frame in range(numExecReps):
        s = frame * dataSize
        e = s + dataSize
        outputData[s:e, :] = kernel.exec(inputData[s:e, :])
    zf = kernel.state

    # Write out header file
    headerFileName = "staticRefCase" + str(testId) + ".h"
    file_io.write_header_file(
        inputData,
        sosCoefs,
        zi,
        outputData,
        zf,
        tifBiquadCascadeCoefs,
        tifBiquadCascadeGains,
        testCase,
        headerFileName,
    )

    return [np.min(outputData), np.max(outputData)]


def gen_test_case(testCase):
    if testCase["testType"] == "STATIC":
        [minY, maxY] = gen_test_case_header_file(testCase)
        print(
            "Test case generation completed for test ID",
            testCase["ID"],
            "w/ min:",
            minY,
            "and max",
            maxY,
        )
    else:
        print(
            "Skipping test case generation for test ID",
            testCase["ID"],
            "as tesType is RANDOM",
        )


# CSV file with paratmeters for all test cases
testCasesCsvFile = "test_cases_list.csv"
testIdList = []  # list of test IDs that we want to generate test data
testIdList += range(1, 688)
# testIdList = [10]

myArgs = TISP_utils.getCmdLineArgs()

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


idatFileName = "../../../DSPLIB/TISP_cascadeBiquad/TISP_cascadeBiquad_idat.cpp"

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
