from __future__ import print_function
from subprocess import call
from sys import platform

import sys
import numpy as np
import csv

sys.path.append("../../common")
import TISP_utils
import file_io

from scipy import signal
import math
import matplotlib.pyplot as plt
import warnings

# Number of coefficients per biquad, including a0 coefficieint
NUM_COEFS_PER_BIQUAD_FULL = 6
# Number of coefficients per biquad, excluding a0 coefficieint
NUM_COEFS_PER_BIQUAD = NUM_COEFS_PER_BIQUAD_FULL - 1
# Number of coefficients per TI biquad
NUM_COEFS_PER_TI_BIQUAD = NUM_COEFS_PER_BIQUAD - 1

# Number of states variables per DF2/TDF2 biquad
NUM_STATES_PER_DF2_BIQUAD = 2

# Number of stages per stage group
NUM_STAGES_PER_STAGE_GROUP = 7

# Cutoff frequencies normalized to Nyquist frequency
LPF_MIN_FREQ = 0.25  # Low pass filter cutoff minium frequency
LPF_RNG_FREQ = 0.25  # Low pass filter cutoff frequency range
HPF_MIN_FREQ = 0.5  # Low pass filter cutoff minium frequency
HPF_RNG_FREQ = 0.25  # Low pass filter cutoff frequency range
BF_LOW_MIN_FREQ = 0.25  # Bandpass/bandstop low cutoff minium frequency
BF_LOW_RNG_FREQ = 0.15  # Bandpass/bandstop low cutoff frequency range
BF_HIGH_MIN_FREQ = 0.6  # Bandpass/bandstop high cutoff minium frequency
BF_HIGH_RNG_FREQ = 0.15  # Bandpass/bandstop high cutoff frequency range

ENABLE_DEBUG_PRINT = 0  # Set to 1 to enable debug print
ENABLE_DEBUG_PLOT = 0  # Set to 1 to enable debug plot


# Define function to catch a warning
def catch_warning(func, *args, **kwargs):
    with warnings.catch_warnings(record=True) as w:
        result = func(*args, **kwargs)
        if w:
            for warning in w:
                print("Warning caught:")
                print(warning.message)
                print(warning.category)
                print(warning.filename)
                print(warning.lineno)
    return result


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


def gen_tif_biquad_coefficients(
    sosCoefs,
    numCoefsPerTiBiquad=NUM_COEFS_PER_TI_BIQUAD,
    numStagesPerStageGroup=NUM_STAGES_PER_STAGE_GROUP,
):
    """Generate TI biquad form coefficients

    Args:
        param1: 3D array of SOS coefficients.
                    Dim1: Number of SOS stages.
                    Dim2: Number of coefficients per stage (6).
                    Dim3: Number of channels.
        param2 (int): Number of coefficients per TI biquad.
        param3 (int): Number of stages per TI stage group.

    Returns:
        numpy.ndarray: 3D array of TI biquad coefficients
                    Dim1: Number of stages.
                    Dim2: Number of TI coefficients per stage (4).
                    Dim3: Number of channels.
        numpy.ndarray: 2D array of TI biquad gains
                    Dim1: Number of stage groups.
                    Dim2: Number of channels.

    """

    # Get parameters
    numStages = sosCoefs.shape[0]
    numChannels = sosCoefs.shape[2]

    ti_biquad_coefs = np.zeros((numStages, numCoefsPerTiBiquad, numChannels))
    for ch in range(numChannels):
        # Calculate biquad coefficients for each channel
        b0_r = 1 / sosCoefs[:, 0, ch]
        ti_biquad_coefs[:, [0, 1], ch] = b0_r[:, None] * sosCoefs[:, [1, 2], ch]
        ti_biquad_coefs[:, [2, 3], ch] = -sosCoefs[:, [4, 5], ch]
        ti_biquad_coefs[:, 0, ch] = (
            ti_biquad_coefs[:, 0, ch] + ti_biquad_coefs[:, 2, ch]
        )
        ti_biquad_coefs[:, 1, ch] = (
            ti_biquad_coefs[:, 1, ch] + ti_biquad_coefs[:, 3, ch]
        )

    # Compute aggregate gain
    numStageGroups = math.ceil(numStages / numStagesPerStageGroup)
    ti_gains = np.zeros((numStageGroups, numChannels))
    for ch in range(numChannels):
        for i in range(numStageGroups):
            start_idx = i * numStagesPerStageGroup
            end_idx = min((i + 1) * numStagesPerStageGroup, numStages)
            stage_group_sos_coefs = sosCoefs[start_idx:end_idx, :, ch]
            ti_gains[i, ch] = np.prod(stage_group_sos_coefs[:, 0])

    return ti_biquad_coefs, ti_gains


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
    dataType = testCase["dataType"]
    dataSize = testCase["dataSize"]
    numChannels = testCase["numChannels"]
    numStages = testCase["numStages"]
    enableNChCoeffs = testCase["enableNChCoeffs"]
    biquadForm = testCase["biquadForm"]
    numExecReps = testCase["numExecReps"]

    if ENABLE_DEBUG_PRINT:
        # Print the test case parameters
        print("Test case parameters:")
        print("ID: ", testId)
        print("dataType: ", dataType)
        print("dataSize: ", dataSize)
        print("numChannels: ", numChannels)
        print("numStages: ", numStages)
        print("enableNChCoeffs: ", enableNChCoeffs)

    if ENABLE_DEBUG_PLOT:
        chanSel = np.random.randint(0, numChannels)  # select random channel for display
        sampFreq = 48000.0  # 48 kHz sampling rate
        dataSize = 48000  # 1 sec of data @ 48 kHz

    if dataType == "float":
        bits = 32
        bytes = bits // 8
    else:
        raise ValueError("Unsupported data type: %s" % dataType)

    #
    # Generate filter coefficients
    #

    # Set number of channels for coefficients generation
    if enableNChCoeffs == "False":
        # All channels use the same coefficients
        filterNumChannels = 1
    else:
        # Different coefficients for each channel
        filterNumChannels = numChannels
    # Initialize filter types
    filterTypes = ["low", "high", "bandpass", "bandstop"]  # butterworth filter types
    filterType = []
    # Initialize filter order and cutoff
    filterOrder = np.zeros(filterNumChannels)
    filterCutoff = np.zeros((filterNumChannels, 2))
    # Initialize biquad coefficients
    bCoefs = np.zeros((filterNumChannels, 2 * numStages + 1))
    aCoefs = np.zeros((filterNumChannels, 2 * numStages + 1))
    # Initialize SOS coefficients
    sosCoefs = np.zeros((numStages, NUM_COEFS_PER_BIQUAD_FULL, filterNumChannels))
    # Loop over channels for filter coefficient generation
    for ch in range(filterNumChannels):
        # Randomly select filter type
        filterType.append(np.random.choice(filterTypes))

        # Set filter order based on filter type
        # Randomly generate filter cutoffs based on filter type
        if filterType[ch] == "low":
            filterOrder[ch] = 2 * numStages
            filterCutoff[ch, 0] = LPF_MIN_FREQ + np.random.rand() * LPF_RNG_FREQ

            # design filter coefficients
            [b, a] = signal.butter(
                filterOrder[ch], filterCutoff[ch, 0], btype=filterType[ch]
            )
            bCoefs[ch, : len(b)] = b
            aCoefs[ch, : len(a)] = a

        elif filterType[ch] == "high":
            filterOrder[ch] = 2 * numStages
            filterCutoff[ch, 0] = HPF_MIN_FREQ + np.random.rand() * HPF_RNG_FREQ

            # design filter coefficients
            [b, a] = signal.butter(
                filterOrder[ch], filterCutoff[ch, 0], btype=filterType[ch]
            )
            bCoefs[ch, : len(b)] = b
            aCoefs[ch, : len(a)] = a

        elif filterType[ch] == "bandpass" or filterType[ch] == "bandstop":
            filterOrder[ch] = numStages
            filterCutoff[ch, 0] = BF_LOW_MIN_FREQ + np.random.rand() * BF_LOW_RNG_FREQ
            filterCutoff[ch, 1] = BF_HIGH_MIN_FREQ + np.random.rand() * BF_HIGH_RNG_FREQ

            # design filter coefficients
            [b, a] = signal.butter(
                filterOrder[ch],
                [filterCutoff[ch, 0], filterCutoff[ch, 1]],
                btype=filterType[ch],
            )

            bCoefs[ch, : len(b)] = b
            aCoefs[ch, : len(a)] = a

        else:
            raise ValueError("Unsupported filter type: %s" % filterType[ch])

        # Design 2nd order sections
        sosChan = signal.tf2sos(bCoefs[ch, : len(b)], aCoefs[ch, : len(a)])
        # sosChan = catch_warning(signal.tf2sos, bCoefs[ch, :len(b)], aCoefs[ch, :len(a)])
        sosCoefs[:, :, ch] = sosChan

    if ENABLE_DEBUG_PRINT:
        print("filterType: ", filterType)
        print("filterOrder: ", filterOrder)
        print("filterCutoff: ", filterCutoff)
        print("bCoefs: ", bCoefs)
        print("bCoefs shape: ", bCoefs.shape)
        print("aCoefs: ", aCoefs)
        print("aCoefs shape: ", aCoefs.shape)
        print("sosCoefs shape: ", sosCoefs.shape)
        print("sosCoefs: ", sosCoefs)

    if ENABLE_DEBUG_PLOT:
        # Plot filter frequency response for selected channel
        if enableNChCoeffs == "False":
            [w, h] = signal.freqz(bCoefs[0, :], aCoefs[0, :], worN=2000)
            plt.plot(w, np.abs(h))
        else:
            [w, h] = signal.freqz(bCoefs[chanSel, :], aCoefs[chanSel, :], worN=2000)
            plt.plot(w, np.abs(h))

    # Generate input data
    inputData = np.random.uniform(
        low=-1.0, high=1.0, size=(numExecReps * dataSize, numChannels)
    ).astype("float")

    if ENABLE_DEBUG_PLOT:
        # Plot output PSD for selected channel
        freqs, psd = signal.periodogram(inputData[:, chanSel], fs=sampFreq)
        psd_db = 10 * np.log10(psd)
        psd_db[0] = np.min(psd_db[1:])  # set DC component to minimum value
        plt.figure()
        plt.plot(freqs, psd_db)
        plt.xlabel("Frequency")
        plt.ylabel("PSD (db/Hz)")

    # Generate output data - filter using 2nd order sections
    # For now, use zero initial conditions
    # Note sosfilt() uses TDF2 for filtering
    zi = np.zeros((numStages, 2, numChannels))
    zf = np.zeros_like(zi)
    outputData = np.zeros_like(inputData)
    if enableNChCoeffs == "False":
        # All channels use the same coefficients
        for ch in range(numChannels):
            outputData[:, ch], zf[:, :, ch] = signal.sosfilt(
                np.ascontiguousarray(sosCoefs[:, :, 0]),
                inputData[:, ch],
                axis=0,
                zi=zi[:, :, ch],
            )
    else:
        # Different coefficients for each channel
        for ch in range(numChannels):
            outputData[:, ch], zf[:, :, ch] = signal.sosfilt(
                np.ascontiguousarray(sosCoefs[:, :, ch]),
                inputData[:, ch],
                axis=0,
                zi=zi[:, :, ch],
            )

    if ENABLE_DEBUG_PLOT:
        # Plot output PSD for selected channel
        freqs, psd = signal.periodogram(outputData[:, chanSel], fs=sampFreq)
        psd_db = 10 * np.log10(psd)
        psd_db[0] = np.min(psd_db[1:])  # set DC component to minimum value
        plt.figure()
        plt.plot(freqs, psd_db)
        plt.xlabel("Frequency")
        plt.ylabel("PSD (db/Hz)")

    if biquadForm == "DSPLIB_BIQUAD_TIF":
        # Generate TI biquad form coefficients
        tifBiquadCascadeCoefs, tifBiquadCascadeGains = gen_tif_biquad_coefficients(
            sosCoefs
        )

        if ENABLE_DEBUG_PRINT:
            print("TIF biquadCascadeCoefs shape: ", tifBiquadCascadeCoefs.shape)
            print("TIF biquadCascadeCoefs: ", tifBiquadCascadeCoefs)
            print("TIF biquadCascadeGains shape: ", tifBiquadCascadeGains.shape)
            print("TIF biquadCascadeGains: ", tifBiquadCascadeGains)

    else:
        tifBiquadCascadeCoefs = None
        tifBiquadCascadeGains = None

    # bypass copies the input to the output, leaving the filter unapplied
    if testCase["bypassMode"]:
        outputData = inputData.copy()

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

    if ENABLE_DEBUG_PLOT:
        plt.show()

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
testIdList += range(1, 679)
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
