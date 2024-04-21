from __future__ import print_function

import os


def write_attributes(f, attributes, stringToWrite):
    f.write("#ifdef WIN32\n")
    f.write(stringToWrite)
    f.write("#else\n")
    f.write(attributes + stringToWrite)
    f.write("#endif\n")
    f.write("{\n")


def write_header_file(input_data, output_data, testCase, headerFileName):
    """Write one test case's input/output tensors as a C header.

    input_data / output_data are already flat float32 arrays in the SuperNode
    2D DMA layout (channel-major rows, numBlocks blocks tiled along the row
    width):
        input  row c = [block0(F) | block1(F) | ...]                length F*numBlocks
        output row c = [block0(W*outBins) | block1(W*outBins) | ...] length W*outBins*numBlocks
    The flattened (C-order) arrays are therefore [ch0 row][ch1 row]... which is
    exactly how the I2dToO2dDiffDims SuperNode walks DDR (height=numChannels,
    width=F*numBlocks / W*outBins*numBlocks).
    """
    testId = testCase["ID"]
    fftSize = int(testCase["fftSize"])
    hopSize = int(testCase["hopSize"])
    numChannels = int(testCase["numChannels"])
    numInputSamples = int(testCase["numInputSamples"])
    onesided = int(testCase["onesided"])
    numBlocks = int(testCase.get("numBlocks", 1))
    windowType = testCase.get("windowType", "hann")

    attributesIn = '__attribute__((section(".staticData"))) static '
    attributesOut = '__attribute__((section(".staticData"))) static '

    outputDir = "../../../../superNode/i2d_to_o2d_generic/TISP_stft_superNode/test_data/"

    if not os.path.exists(outputDir):
        os.makedirs(outputDir)

    with open(outputDir + headerFileName, "w") as f:
        f.write(
            "/* auto-generated test data for TISP_stft_superNode, test case %s */\n" % testId
        )
        f.write(
            "/* fftSize=%d hopSize=%d numCh=%d numInputSamples=%d onesided=%d numBlocks=%d windowType=%s */\n\n"
            % (fftSize, hopSize, numChannels, numInputSamples, onesided, numBlocks, windowType)
        )

        write_attributes(
            f, attributesIn, "float " + "staticRefInCase" + str(testId) + "[]=\n"
        )
        for idx in range(0, len(input_data)):
            if idx > 0 and idx % 8 == 0:
                f.write("\n")
            f.write("%.10ef, " % input_data[idx])
        f.write("\n};\n\n")

        write_attributes(
            f, attributesOut, "float " + "staticRefOutCase" + str(testId) + "[]=\n"
        )
        for idx in range(0, len(output_data)):
            if idx > 0 and idx % 8 == 0:
                f.write("\n")
            f.write("%.10ef, " % output_data[idx])
        f.write("\n};\n\n")
