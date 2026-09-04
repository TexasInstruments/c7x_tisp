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


def write_header_file(input_data, output_data, testCase, headerFileName):
    """Write one test case's input waveform + expected log-mel output to a .h file.

    Mirrors TISP_stft/file_io.py, but the output array is the flattened log-mel
    spectrogram (numBlocks * numWindows * numMelBins * numChannels reals) rather
    than the complex-interleaved STFT bins.
    """
    testId = testCase["ID"]
    fftSize = int(testCase["fftSize"])
    hopSize = int(testCase["hopSize"])
    numChannels = int(testCase["numChannels"])
    numInputSamples = int(testCase["numInputSamples"])
    numMelBins = int(testCase["numMelBins"])
    numBlocks = int(testCase.get("numBlocks", 1))
    windowType = testCase.get("windowType", "hann")

    attributesIn = '__attribute__((section(".staticData"))) static '
    attributesOut = '__attribute__((section(".staticData"))) static '

    outputDir = "../../../../superNode/i2d_to_o2d_generic/TISP_logMelSpectrum_superNode/test_data/"

    if not os.path.exists(outputDir):
        os.makedirs(outputDir)

    with open(outputDir + headerFileName, "w") as f:
        f.write(
            "/* auto-generated test data for TISP_logMelSpectrum, test case %s */\n" % testId
        )
        f.write(
            "/* fftSize=%d hopSize=%d numCh=%d numInputSamples=%d numMelBins=%d numBlocks=%d windowType=%s */\n\n"
            % (fftSize, hopSize, numChannels, numInputSamples, numMelBins, numBlocks, windowType)
        )

        # input array (waveform)
        write_attributes(
            f,
            attributesIn,
            "float " + "staticRefInCase" + str(testId) + "[]=\n",
        )
        for idx in range(0, len(input_data)):
            if idx > 0 and idx % 8 == 0:
                f.write("\n")
            f.write("%.10ef, " % input_data[idx])
        f.write("\n};\n\n")

        # output array (log-mel spectrogram)
        write_attributes(
            f,
            attributesOut,
            "float " + "staticRefOutCase" + str(testId) + "[]=\n",
        )
        for idx in range(0, len(output_data)):
            if idx > 0 and idx % 8 == 0:
                f.write("\n")
            f.write("%.10ef, " % output_data[idx])
        f.write("\n};\n\n")
