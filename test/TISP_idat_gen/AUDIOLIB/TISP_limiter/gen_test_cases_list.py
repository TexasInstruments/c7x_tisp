#!/usr/bin/env python3
"""
Script to generate test_cases_list.csv for TISP_limiter
This script generates combinations of numChannels, blockSize, and pitch sizes
with isInterleaved always set to true
"""

import csv
import os
import itertools

# Fixed parameters
TEST_PATTERN = "STATIC"
OUTPUT_DATA_LOCATION = "AUDIOLIB_TEST_OUTPUT_MSMC"
NUM_REPS = 1
DATA_TYPE = "AUDIOLIB_FLOAT32"
IS_INTERLEAVED = 1

# Define the parameters for the test cases
NUM_CHANNELS = [1, 3, 8, 12, 16]  # Number of channels
BLOCK_SIZES = [32, 64, 128, 256, 512, 1024, 2048, 1, 23, 79, 275, 567]  # Block sizes


def generate_test_cases_list():
    """Generate test_cases_list.csv with combinations of parameters"""

    output_file = "test_cases_list.csv"

    # Create the header for the CSV file
    header = [
        "testId",
        "testPattern",
        "outputDataLocation",
        "numReps",
        "numChannels",
        "blockSize",
        "dataType",
        "pitchX",
        "pitchS",
        "pitchY",
        "isInterleaved",
    ]

    # Generate test cases
    test_cases = []
    test_id = 1

    # Generate combinations of numChannels and blockSize
    for num_channels, block_size in itertools.product(NUM_CHANNELS, BLOCK_SIZES):
        # For each combination, generate different pitch sizes

        if IS_INTERLEAVED:
            pitch_x = pitch_s = pitch_y = 4 * num_channels

        else:
            pitch_x = pitch_s = pitch_y = 4 * block_size

        # Create the test case
        test_case = [
            test_id,
            TEST_PATTERN,
            OUTPUT_DATA_LOCATION,
            NUM_REPS,
            num_channels,
            block_size,
            DATA_TYPE,
            pitch_x,
            pitch_s,
            pitch_y,
            IS_INTERLEAVED,
        ]

        test_cases.append(test_case)
        test_id += 1

    # Write to CSV file
    with open(output_file, "w", newline="") as csvfile:
        writer = csv.writer(csvfile)
        writer.writerow(header)
        writer.writerows(test_cases)

    print(f"Generated {len(test_cases)} test cases in {output_file}")


if __name__ == "__main__":
    generate_test_cases_list()
