"""
TISP Mixer Processing Functions

This module contains the core mixer computation logic.
"""

import numpy as np


def mixer_process(input_signals, coeffs, num_inputs, num_outputs):
    """
    Mixer implementation: M inputs x N outputs.
    Works with PLANAR format: (Channels, Samples)

    Args:
        input_signals: 2D array of shape (num_inputs, num_samples) - PLANAR format
        coeffs: 1D array of length M*N where coeffs[m + n*M] is gain from input m to output n
        num_inputs: M (number of input channels)
        num_outputs: N (number of output channels)

    Returns:
        output_signals: 2D array of shape (num_outputs, num_samples) - PLANAR format
    """
    # Ensure input is 2D
    if input_signals.ndim == 1:
        input_signals = input_signals.reshape(1, -1)

    num_samples = input_signals.shape[1]
    dtype = input_signals.dtype

    # Initialize output in planar format
    output_signals = np.zeros((num_outputs, num_samples), dtype=dtype)

    # Process: Output[n, :] = sum over m of (coeffs[m + n*M] * Input[m, :])
    for n in range(num_outputs):  # For each output channel
        for m in range(num_inputs):  # For each input channel
            # Calculate index in flat coeff array
            # coeff order: [In0Out0, In1Out0... InMOut0, In0Out1...]
            coeff_idx = m + n * num_inputs

            if coeff_idx < len(coeffs):
                output_signals[n, :] += coeffs[coeff_idx] * input_signals[m, :]

    return output_signals


def generate_random_coeffs(num_inputs, num_outputs, seed=None):
    """
    Generate random mixer coefficients.
    Returns: 1D array of length M*N
    """
    if seed is not None:
        np.random.seed(seed)

    total_coeffs = num_inputs * num_outputs
    coeffs = np.random.uniform(-1.0, 1.0, total_coeffs)

    return coeffs
