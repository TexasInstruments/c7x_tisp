import itertools
import csv

dataSize_values = [128, 256, 512, 1024]
cascadeForm = 1
cascadeStages = 3
cascadeFilterSize = cascadeStages * 5
cascadeFilterVarSize = cascadeStages * cascadeForm
numChannels_values = [16, 32, 64]
enableNChCoeffs = 0

with open("./test_cases_list.csv", "w", newline="") as csvfile:
    fieldNames = [
        "ID",
        "testType",
        "dataSize",
        "dTypeIn",
        "numChannels",
        "numStages",
        "biquadForm",
        "filterSize",
        "filterVarSize",
        "dataPitch",
        "filterCoeffPitch",
        "filterVarPitch",
        "enableNChCoeffs",
        "enableCalcCoeffs",
        "numReps",
        "demoCase",
        "outputDataLocation",
    ]
    writer = csv.DictWriter(csvfile, fieldnames=fieldNames)

    writer.writeheader()
    id = 1
    for dataSize in dataSize_values:
        for numChannels in numChannels_values:
            writer.writerow(
                {
                    "ID": id,
                    "testType": "STATIC",
                    "dataSize": dataSize,
                    "dTypeIn": "float",
                    "numChannels": numChannels,
                    "numStages": cascadeStages,
                    "biquadForm": cascadeForm,
                    "filterSize": cascadeFilterSize,
                    "filterVarSize": cascadeFilterVarSize * numChannels,
                    "dataPitch": 4 * numChannels,
                    "filterCoeffPitch": 4 * numChannels,
                    "filterVarPitch": 4 * numChannels,
                    "enableNChCoeffs": enableNChCoeffs,
                    "numReps": 1,
                    "demoCase": "False",
                    "outputDataLocation": "DSPLIB_TEST_OUTPUT_HEAP",
                }
            )
            id += 1
