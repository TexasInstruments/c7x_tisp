import itertools
import csv

dataSize_values = [256, 512, 1024, 2048]
# dataSize_values = [512]

with open(
    "./test/TISP_idat_gen/superNode/TISP_matMul_superNode/test_cases_list.csv",
    "w",
    newline="",
) as csvfile:
    fieldnames = [
        "ID",
        "testType",
        "M",
        "N",
        "K",
        "dTypeIn",
        "dTypeOut",
        "pitchA",
        "pitchB",
        "pitchOut",
        "numReps",
        "demoCase",
        "outputDataLocation",
    ]
    writer = csv.DictWriter(csvfile, fieldnames=fieldnames)

    writer.writeheader()
    id = 1
    for dataSize in dataSize_values:
        m = dataSize
        n = dataSize
        k = dataSize
        pitchA = 4 * dataSize
        pitchB = 4 * dataSize
        pitchOut = 4 * dataSize
        writer.writerow(
            {
                "ID": id,
                "testType": "STATIC",
                "M": m,
                "N": n,
                "K": k,
                "dTypeIn": "float",
                "dTypeOut": "float",
                "pitchA": pitchA,
                "pitchB": pitchB,
                "pitchOut": pitchOut,
                "numReps": 1,
                "demoCase": "False",
                "outputDataLocation": "DSPLIB_TEST_OUTPUT_HEAP",
            }
        )
        id += 1
