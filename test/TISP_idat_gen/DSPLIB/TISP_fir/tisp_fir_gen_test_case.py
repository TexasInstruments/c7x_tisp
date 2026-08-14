import itertools
import csv

dataSize_values = [32, 64, 128, 256, 512, 1024]
filterSize_values = [16, 32, 64, 128, 1024, 2048]
batchSize_values = [1, 4, 8]
enableNChCoefs_values = [1]
enableMMA_values = [0, 1]
Q_values = [23]

with open("./test_cases_list.csv", "w", newline="") as csvfile:
    fieldnames = [
        "ID",
        "testType",
        "dataSize",
        "numBlocks",
        "dTypeIn",
        "filterSize",
        "dTypeFilter",
        "dTypeOut",
        "batchSize",
        "stateSize",
        "pitchX",
        "pitchH",
        "pitchY",
        "enableNChCoefs",
        "enableMMA",
        "enableQ",
        "Q",
        "numReps",
        "demoCase",
        "outputDataLocation",
        "bypassMode",
    ]
    writer = csv.DictWriter(csvfile, fieldnames=fieldnames)

    writer.writeheader()
    id = 1
    for dataSize, filterSize in [
        (ds, fs) for ds in dataSize_values for fs in filterSize_values if ds > fs
    ]:
        for batchSize in batchSize_values:
            for enableNChCoefs in enableNChCoefs_values:
                for enableMMA in enableMMA_values:
                    for Q in Q_values:
                        enableQ = enableMMA
                        stateSize = dataSize + filterSize - 1
                        pitchX = 4 * dataSize
                        pitchH = 4 * filterSize
                        pitchY = 4 * dataSize
                        writer.writerow(
                            {
                                "ID": id,
                                "testType": "STATIC",
                                "dataSize": dataSize,
                                "numBlocks": 4,
                                "dTypeIn": "float",
                                "filterSize": filterSize,
                                "dTypeFilter": "float",
                                "dTypeOut": "float",
                                "batchSize": batchSize,
                                "stateSize": stateSize,
                                "pitchX": pitchX,
                                "pitchH": pitchH,
                                "pitchY": pitchY,
                                "enableNChCoefs": enableNChCoefs,
                                "enableMMA": enableMMA,
                                "enableQ": enableQ,
                                "Q": Q,
                                "numReps": 1,
                                "demoCase": "False",
                                "outputDataLocation": "DSPLIB_TEST_OUTPUT_HEAP",
                                "bypassMode": 0,
                            }
                        )
                        id += 1
