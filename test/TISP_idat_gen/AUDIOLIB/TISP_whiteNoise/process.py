import numpy as np


def generate_white_noise(
    numSamples, numExecReps, seed, range_val, dType="AUDIOLIB_FLOAT32"
):
    """
    Generate white noise across multiple frames, maintaining PRNG state continuity.

    Args:
        numSamples (int): Number of samples per frame.
        numExecReps (int): Number of frames to generate.
        seed (int): Initial LCG seed.
        range_val (float): Output range [-range_val, range_val].
        dType (str): "AUDIOLIB_FLOAT32" or "AUDIOLIB_FLOAT64".

    Returns:
        numpy.ndarray: Array of (numSamples * numExecReps) samples.
    """
    # Choose dtype
    target_dtype = np.float32 if dType == "AUDIOLIB_FLOAT32" else np.float64

    # Constants
    a = 1664525
    c = 1013904223
    m = 2**32

    totalSamples = numSamples * numExecReps
    out = np.zeros(totalSamples, dtype=target_dtype)
    x = seed

    for frame in range(numExecReps):
        for i in range(numSamples):
            x = (a * x + c) % m
            normalized_val = target_dtype(x / m)
            out[frame * numSamples + i] = (2 * range_val * normalized_val) - range_val

    return out
