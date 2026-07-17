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

    headerFileName = "staticRefCase" + str(testId) + ".h"

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
    MATHLIB_dType = ""
    if dType[0] == "f":
        MATHLIB_dType = "MATHLIB_FLOAT32"
    elif dType[0] == "d":
        MATHLIB_dType = "MATHLIB_FLOAT64"

    return MATHLIB_dType


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
