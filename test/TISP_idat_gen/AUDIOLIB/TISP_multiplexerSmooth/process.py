import numpy as np


def fader(in0, in1, fadeIn, fadeOut):
    out = in0 * fadeIn + in1 * fadeOut
    return out
