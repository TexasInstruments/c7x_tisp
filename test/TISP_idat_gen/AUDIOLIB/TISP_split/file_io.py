from __future__ import print_function
from subprocess import call
from sys import platform
import os
import sys
import numpy as np
import csv


def write_attributes(f, attributes, stringToWrite):
    f.write("#ifdef WIN32\n")
    f.write(stringToWrite)
    f.write("#else\n")
    f.write(attributes + stringToWrite)
    f.write("#endif\n")
    f.write("{\n")

def write_header_file(in0, out0, num_out, testCase, headerFileName, dataFormat="INTERLEAVED"):
    """Write split test input and output data to C header file.

    Writes reference data for verifying split operation:
    - For INTERLEAVED: Input [samples, totalChannels], Outputs per-sample interleaved
    - For DEINTERLEAVED: Input [totalChannels, samples], Outputs per-channel deinterleaved

    Args:
        in0: Input data array [samples, totalChannels] or [totalChannels, samples]
        out0: List of output arrays, each [samples*channelsPerOutput] or [samples*channelsPerOutput]
        num_out: Number of outputs
        testCase: Test parameters dict
        headerFileName: Output header filename
        dataFormat: "INTERLEAVED" or "DEINTERLEAVED"
    """
    testId = testCase["ID"]
    samples = int(testCase["samples"])
    totalChannels = int(testCase["totalInputChannels"])    # C_total - total input channels
    dType = testCase["dType"]
    numOutputs = int(testCase["numOutputs"])
    # Per-output channel counts (each output may have a different channel count)
    outChannelsList = testCase["outChannelsList"]

    attributesIn0 = '__attribute__((section(".staticData"))) __attribute__((unused)) static '
    attributesIn1 = '__attribute__((section(".staticData"))) __attribute__((unused)) static '
    attributesOut = '__attribute__((section(".staticData"))) __attribute__((unused)) static '

    outputDir = "../../../AUDIOLIB/TISP_split/test_data/"
    
    # Create the directory if it doesn't exist
    os.makedirs(outputDir, exist_ok=True)
    with open(outputDir + headerFileName, "w") as f:
        write_attributes(
            f,
            attributesIn0,
            dType + " " + "staticRefInCase" + str(testId) + "[]=\n",
        )

        if dataFormat == "DEINTERLEAVED":
            # in0 shape is (totalChannels, samples) - deinterleaved
            for ch in range(totalChannels):
                for s in range(samples):
                    print("{:>8.1f}".format(in0[ch, s]), file=f, end=", ")
                print(" ", file=f)
        else:  # INTERLEAVED
            # in0 shape is (samples, totalChannels)
            for dimY in range(samples):
                for dimX in range(totalChannels):
                    print("{:>2}".format(in0[dimY, dimX]), file=f, end=", ")
                print(" ", file=f)

        f.write("\n};")
        f.write("\n\n")

    with open(outputDir + headerFileName, "a") as f:
        for idx, out in enumerate(out0):
            write_attributes(
                f,
                attributesOut + "__attribute__((unused)) ",
                dType + " " + "staticRefOut" + str(idx) + "Case" + str(testId) + "[]=\n",
            )

            channelsThisOutput = outChannelsList[idx]
            if dataFormat == "DEINTERLEAVED":
                # output is deinterleaved: (channelsThisOutput, samples)
                out_reshaped = np.array(out).reshape((channelsThisOutput, samples))
                # Print per-channel, per-sample
                for ch_idx in range(channelsThisOutput):
                    for sample_idx in range(samples):
                        print("{:>8.1f}".format(out_reshaped[ch_idx, sample_idx]), file=f, end=", ")
                    print(" ", file=f)
            else:  # INTERLEAVED
                # Reshape output to (samples, channelsThisOutput) for proper formatting
                out_reshaped = np.array(out).reshape((samples, channelsThisOutput))
                # Print row by row (sample by sample)
                for sample_idx in range(samples):
                    for ch_idx in range(channelsThisOutput):
                        print("{:>2}".format(out_reshaped[sample_idx, ch_idx]), file=f, end=", ")
                    print(" ", file=f)

            f.write("\n};\n\n")     

    if platform == "linux" or platform == "linux2":
        call(
            [
                "indent",
                "-nut",
                "-i3",
                # "-c55",
                "-l100",
                outputDir + headerFileName,
            ]
        )
        call(["rm", outputDir + headerFileName + "~"])  
