#!/usr/bin/env python3
"""
Script to generate test_cases_list.csv for TISP_downwardExpander
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

# Define the parameters for the test cases
NUM_CHANNELS = [1, 3, 8, 12, 16]  # Number of channels
BLOCK_SIZES = [32, 64, 128, 256, 512, 1024, 2048, 1, 23, 79, 275, 567]  # Block sizes

# Random-signal cases appended after the designed cases: padded row pitches and several
# consecutive exec() calls that carry the gain and hold state across block boundaries.
# (numChannels, blockSize, numBlocks, pitchX, pitchS, pitchY, bypassMode)
PITCH_AND_STREAM_CASES = [
    (3, 79, 1, 16, 20, 24, BYPASS_OFF),
    (8, 64, 8, 32, 32, 32, BYPASS_OFF),
    (12, 23, 4, 64, 56, 52, BYPASS_OFF),
    (3, 32, 2, 16, 12, 20, BYPASS_ON),
]


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
    # which downwardExpander.py cross-checks against these rows, so the two cannot drift apart.
    #
    # The ids in designed_cases.py are fixed, and downwardExpander.py dispatches on them, so they have
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
        num_blocks = case.get("numBlocks", 1)
        if case["blockSize"] % num_blocks != 0:
            raise ValueError(
                f"designed case {designed_id}: blockSize {case['blockSize']} is not a multiple of "
                f"numBlocks {num_blocks}"
            )
        block_size = case["blockSize"] // num_blocks

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
                num_blocks,
            ]
        )

        print(f"  designed case {designed_id}: {case['description']}")
        test_id = designed_id + 1

    # Append the padded-pitch and multi-block cases after the designed ones.
    for num_channels, block_size, num_blocks, pitch_x, pitch_s, pitch_y, bypass in PITCH_AND_STREAM_CASES:
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
                bypass,
                num_blocks,
            ]
        )
        print(f"  pitch/stream case {test_id}: {num_channels} ch x {block_size} x {num_blocks} blocks")
        test_id += 1

    # Write to CSV file
    with open(output_file, "w", newline="") as csvfile:
        writer = csv.writer(csvfile)
        writer.writerow(header)
        writer.writerows(test_cases)

    print(f"Generated {len(test_cases)} test cases in {output_file}")


if __name__ == "__main__":
    generate_test_cases_list()
