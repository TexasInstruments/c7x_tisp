#!/usr/bin/env python3
# Copyright (C) 2026 Texas Instruments Incorporated
#
# SPDX-License-Identifier: LicenseRef-TI-TFL

"""
Script to generate test_cases_list.csv for TISP_limiter
This script generates combinations of numChannels, blockSize, and pitch sizes
"""

import csv
import os
import itertools

import designed_cases

# Fixed parameters
TEST_PATTERN = "STATIC"
OUTPUT_DATA_LOCATION = "AUDIOLIB_TEST_OUTPUT_MSMC"
NUM_REPS = 1
DATA_TYPE = "AUDIOLIB_FLOAT32"
BYPASS_OFF = 0
BYPASS_ON = 1

# Bypass cases: the node copies input to output and never runs the gain computer, so these
# only need to prove the copy respects the geometry. Same three shapes dev uses.
BYPASS_CASES = [
    (1, 32),
    (8, 1024),
    (16, 567),
]

# Multi-block cases: (numChannels, blockSize, numBlocks). The kernel carries its envelope
# (pGainState) from block to block, so these run the blocks back to back and compare every
# block, which a single-block case cannot do. Appended after the designed cases so existing
# test ids stay put.
MULTIBLOCK_CASES = [
    (1, 32, 4),
    (2, 32, 4),
    (3, 64, 3),
    (8, 128, 4),
    (16, 23, 5),
    (12, 275, 3),
]

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
        "bypassMode",
        "numBlocks",
    ]

    # Generate test cases
    test_cases = []
    test_id = 1

    # Generate combinations of numChannels and blockSize
    for num_channels, block_size in itertools.product(NUM_CHANNELS, BLOCK_SIZES):
        # For each combination, generate different pitch sizes

        pitch_x = pitch_s = pitch_y = 4 * num_channels

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
            BYPASS_OFF,
            1,
        ]

        test_cases.append(test_case)
        test_id += 1

    # Append the bypass cases, continuing the geometry-matrix id sequence.
    for num_channels, block_size in BYPASS_CASES:
        pitch_x = pitch_s = pitch_y = 4 * num_channels

        test_cases.append(
            [
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
                BYPASS_ON,
                1,
            ]
        )
        print(f"  bypass case {test_id}: {num_channels} ch x {block_size}")
        test_id += 1

    # Append the designed algorithm cases. Their geometry comes from designed_cases.py,
    # which limiter.py cross-checks against these rows, so the two cannot drift apart.
    #
    # The ids in designed_cases.py are fixed, and limiter.py dispatches on them, so they have
    # to line up with wherever the sequence has got to. Check that here rather than letting a
    # newly added geometry or bypass case silently collide with a designed case.
    designed_ids = sorted(designed_cases.DESIGNED_CASES)
    expected_ids = list(range(test_id, test_id + len(designed_ids)))
    if designed_ids != expected_ids:
        raise ValueError(
            f"designed case ids {designed_ids} are not the next contiguous run {expected_ids}. "
            f"The geometry matrix and bypass cases now end at id {test_id - 1}; renumber "
            f"DESIGNED_CASES in designed_cases.py to match."
        )

    for designed_id in designed_ids:
        case = designed_cases.DESIGNED_CASES[designed_id]
        num_channels = case["numChannels"]
        block_size = case["blockSize"]

        pitch_x = pitch_s = pitch_y = 4 * num_channels

        test_cases.append(
            [
                designed_id,
                TEST_PATTERN,
                OUTPUT_DATA_LOCATION,
                NUM_REPS,
                num_channels,
                block_size,
                DATA_TYPE,
                pitch_x,
                pitch_s,
                pitch_y,
                BYPASS_OFF,
                1,
            ]
        )

        print(f"  designed case {designed_id}: {case['description']}")

    # Append the multi-block cases after the designed ones.
    test_id = max(designed_ids) + 1
    for num_channels, block_size, num_blocks in MULTIBLOCK_CASES:
        pitch_x = pitch_s = pitch_y = 4 * num_channels

        test_cases.append(
            [
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
                BYPASS_OFF,
                num_blocks,
            ]
        )
        print(
            f"  multi-block case {test_id}: {num_channels} ch x {block_size} x {num_blocks} blocks"
        )
        test_id += 1

    # Write to CSV file
    with open(output_file, "w", newline="") as csvfile:
        writer = csv.writer(csvfile)
        writer.writerow(header)
        writer.writerows(test_cases)

    print(f"Generated {len(test_cases)} test cases in {output_file}")


if __name__ == "__main__":
    generate_test_cases_list()
