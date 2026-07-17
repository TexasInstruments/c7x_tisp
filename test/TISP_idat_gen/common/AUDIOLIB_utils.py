from __future__ import print_function
from subprocess import call
from sys import platform

import sys
import numpy as np
import csv
import argparse


def gen_ifdefs(idatFile, testCase):
    """Generate C++ ifdefs macros for a given test case

    Args:
        param1 (file object): Input file object to idat.c file
        param2 (dict object): Dict object with test case parameter

    """
    testId = testCase["ID"]
    demoCase = testCase["demoCase"]
    if "testCategory" in testCase:
        testCategory = testCase["testCategory"]

    headerFileName = "staticRefCase" + str(testId) + ".h"

    if demoCase == "true":
        idatFile.write(
            "#if (defined(ALL_TEST_CASES) || (TEST_CASE == %d) || defined(DEMO_CASE))\n"
            % (int(testId))
        )
    else:
        if "testCategory" in testCase:
            idatFile.write(
                "#if (defined(ALL_TEST_CASES) || (TEST_CASE == %d) || (TEST_CATEGORY == %d))\n"
                % (int(testId), int(testCategory))
            )
        else:
            idatFile.write(
                "#if (defined(ALL_TEST_CASES) || (TEST_CASE == %d))\n" % (int(testId))
            )

    idatFile.write('#include "test_data/%s"\n' % (headerFileName))
    idatFile.write("#endif\n\n")


def shiftAndRound(inVal, shift):
    np.right_shift(inVal, shift - 1, out=inVal, where=shift != 0)
    np.add(inVal, 1, out=inVal, where=shift != 0)
    np.right_shift(inVal, 1, out=inVal, where=shift != 0)
    return inVal


def resolve_dType(dType):
    AUDIOLIB_dType = ""
    if dType[0] == "i":
        if dType[3] == "8":
            AUDIOLIB_dType = "AUDIOLIB_INT" + dType[3]
        else:
            AUDIOLIB_dType = "AUDIOLIB_INT" + dType[3] + dType[4]
    elif dType[0] == "u":
        if dType[4] == "8":
            AUDIOLIB_dType = "AUDIOLIB_UINT" + dType[4]
        else:
            AUDIOLIB_dType = "AUDIOLIB_UINT" + dType[4] + dType[5]
    elif dType[0] == "f":
        AUDIOLIB_dType = "AUDIOLIB_FLOAT32"
    elif dType[0] == "d":
        AUDIOLIB_dType = "AUDIOLIB_FLOAT64"

    return AUDIOLIB_dType


def minMaxVal(dType):
    if dType[0] == "i" or dType[0] == "u":
        [minVal, maxVal] = [np.finfo("float32").min, np.finfo("float32").max]
    elif dType[0] == "f":
        [minVal, maxVal] = [np.finfo("float32").min, np.finfo("float32").max]
    elif dType[0] == "d":
        [minVal, maxVal] = [np.finfo("double").min, np.finfo("double").max]

    return [minVal, maxVal]


def getCmdLineArgs():

    parser = argparse.ArgumentParser()

    parser.add_argument(
        "-a",
        action="store_true",
        default=False,
        dest="allCases",
        help="Set to generate all cases",
    )

    myArgs = parser.parse_args()

    return myArgs


def generate_sine_wave(
    amplitude=1.0, num_points=64, num_channels=1, frequency=1.0, phase_shift=None
):
    """
    Generate sine waves for multiple channels with the specified parameters.

    Parameters:
    - amplitude: The amplitude of the sine wave(s). Can be a scalar or array of size num_channels (default: 1.0)
    - num_points: The number of points in the sine wave (default: 64)
    - num_channels: Number of channels to generate (default: 1)
    - frequency: The frequency of the sine wave(s). Can be a scalar or array of size num_channels (default: 1.0)
    - phase_shift: Phase shift for each channel in radians. Can be None or array of size num_channels (default: None)

    Returns:
    - x: The x values (angles in radians)
    - sin_wave: The sine wave values with shape (num_channels, num_points)
    """
    # Convert scalar parameters to arrays if needed
    if np.isscalar(amplitude):
        amplitude = np.full(num_channels, amplitude)
    if np.isscalar(frequency):
        frequency = np.full(num_channels, frequency)
    if phase_shift is None:
        phase_shift = np.zeros(num_channels)

    # Generate evenly spaced points from 0 to 2π (one complete cycle)
    x = np.linspace(0, 2 * np.pi, num_points, endpoint=False)

    # Initialize output array for multiple channels
    sin_wave = np.zeros((num_channels, num_points))

    # Generate sine values for each channel with the specified parameters
    for ch in range(num_channels):
        # Scale x by frequency for this channel
        x_scaled = x * frequency[ch]
        # Generate sine wave with amplitude and phase shift for this channel
        sin_wave[ch] = amplitude[ch] * np.sin(x_scaled + phase_shift[ch])

    return x, sin_wave
