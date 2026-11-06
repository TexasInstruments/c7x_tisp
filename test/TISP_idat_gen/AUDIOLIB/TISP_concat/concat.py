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
    
    # Accepts numpy names ("float32", "float64", ...) and the legacy C names
    # ("float", "double").
    np_dtype = {"float": np.float32, "double": np.float64}.get(dtype) or np.dtype(dtype)
    
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


class Concat:
    """Reference model of the concat kernel (stack input channels)."""

    def __init__(self, *, num_samples=None, in_dtype=None, is_interleaved=False, **_):
        self.num_samples = num_samples
        self.in_dtype = in_dtype
        self.is_interleaved = is_interleaved
        self.init()

    def init(self):
        """Normalise the layout flag once (called by the constructor)."""
        self.is_interleaved = bool(self.is_interleaved)

    def exec(self, *inputs: np.ndarray) -> np.ndarray:
        """Concatenate the input buffers along the channel axis."""
        return concat_aggregator(
            list(inputs), self.is_interleaved, self.is_interleaved
        )

    def generate_inputs(self, in_channels_list, min_val=-10.0, max_val=10.0, seed=None):
        """SDK test signal: one random array per input (for gen_data.py only)."""
        return generate_inputs(
            in_channels_list=in_channels_list,
            num_samples=self.num_samples,
            dtype=self.in_dtype,
            is_input_interleave=self.is_interleaved,
            min_val=min_val,
            max_val=max_val,
            seed=seed,
        )
