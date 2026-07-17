import numpy as np
from typing import List, Tuple


def concat_aggregator(
    individual_inputs: List[np.ndarray],
    is_input_interleave: bool,
    is_output_interleave: bool
) -> np.ndarray:
    
    if len(individual_inputs) == 0:
        raise ValueError("At least one input buffer is required")
    
    # Convert all inputs to planar (channels, samples) format for aggregation
    planar_inputs = []
    for inp in individual_inputs:
        if is_input_interleave:
            # Input shape: (samples, channels) -> transpose to (channels, samples)
            planar_inputs.append(inp.T)
        else:
            # Input shape: (channels, samples) -> already planar
            planar_inputs.append(inp)
    
    # Aggregate: stack all channels vertically
    # Result shape: (total_channels, samples)
    merged_deinterleaved = np.vstack(planar_inputs)
    
    # Convert to output format
    if is_output_interleave:
        # Output shape: (samples, total_channels)
        return merged_deinterleaved.T
    else:
        # Output shape: (total_channels, samples)
        return merged_deinterleaved


def generate_inputs(
    in_channels_list: List[int],
    num_samples: int,
    dtype: str,
    is_input_interleave: bool,
    min_val: float = -10.0,
    max_val: float = 10.0,
    seed: int = None
) -> List[np.ndarray]:

    if seed is not None:
        np.random.seed(seed)
    
    np_dtype = np.float32 if dtype.startswith('f') else np.float64
    
    individual_inputs = []
    for num_ch in in_channels_list:
        if is_input_interleave:
            # Interleaved: (samples, channels)
            shape = (num_samples, num_ch)
        else:
            # Non-interleaved (planar): (channels, samples)
            shape = (num_ch, num_samples)
        
        inp = np.random.uniform(low=min_val, high=max_val, size=shape).astype(np_dtype)
        individual_inputs.append(inp)
    
    return individual_inputs


def get_output_shape(
    in_channels_list: List[int],
    num_samples: int,
    is_output_interleave: bool
) -> Tuple[int, int]:

    total_channels = sum(in_channels_list)
    
    if is_output_interleave:
        return (num_samples, total_channels)
    else:
        return (total_channels, num_samples)
