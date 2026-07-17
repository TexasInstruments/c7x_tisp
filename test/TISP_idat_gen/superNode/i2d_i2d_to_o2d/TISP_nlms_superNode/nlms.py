import numpy as np


def nlms_filter_interleaved(
    input_signal_interleaved,
    desired_signal_interleaved,
    num_channels,
    num_samples_per_channel,
    filter_length,
    step_size,
    block_size,
):
    """
    Block NLMS for interleaved data with block-based updates.
    """
    dtype = input_signal_interleaved.dtype.type

    # Ensure total samples is a multiple of block size
    if num_samples_per_channel % block_size != 0:
        raise ValueError(
            f"num_samples_per_channel ({num_samples_per_channel}) must be multiple of block_size ({block_size})"
        )

    num_blocks = int(num_samples_per_channel // block_size)

    w = np.zeros((num_channels, filter_length), dtype=dtype)
    x_buffer = np.zeros((num_channels, filter_length), dtype=dtype)
    write_idx = np.zeros(num_channels, dtype=int)

    # Energy calculated per sample now
    energy = np.zeros(num_channels, dtype=dtype)
    regularization = dtype(1e-3)
    output_signal_interleaved = np.zeros_like(input_signal_interleaved, dtype=dtype)

    # Iterate over blocks (Frames)
    for block in range(num_blocks):
        accum_update = np.zeros((num_channels, filter_length), dtype=dtype)

        # Iterate samples within the block
        for s in range(block_size):
            sample_idx = block * block_size + s

            for ch in range(num_channels):
                interleaved_idx = sample_idx * num_channels + ch
                x_n = dtype(input_signal_interleaved[interleaved_idx])

                widx = write_idx[ch]

                # 1. Update State
                x_buffer[ch, widx] = x_n

                # 2. Brute Force Energy (Matches C7x)
                energy[ch] = dtype(0.0)
                for k in range(filter_length):
                    energy[ch] += x_buffer[ch, k] ** 2

                # 3. compute y_n
                y_n = dtype(0.0)
                idx = widx
                for i in range(filter_length):
                    y_n += x_buffer[ch, idx] * w[ch, filter_length - 1 - i]
                    idx = (idx + 1) % filter_length

                output_signal_interleaved[interleaved_idx] = y_n

                e_n = dtype(desired_signal_interleaved[interleaved_idx]) - y_n

                # 4. Step Factor with Clamping
                energy_clamped = max(energy[ch], dtype(1e-6))
                step_factor = (dtype(2.0) * dtype(step_size) * e_n) / (
                    energy_clamped + regularization
                )

                idx = widx
                for i in range(filter_length):
                    accum_update[ch, filter_length - 1 - i] += (
                        step_factor * x_buffer[ch, idx]
                    )
                    idx = (idx + 1) % filter_length

                write_idx[ch] = (write_idx[ch] + 1) % filter_length

        # Update weights at the end of the block (Frame)
        w += accum_update

    return output_signal_interleaved, w


def nlms_filter(input_signal, desired_signal, filter_length, step_size, block_size):
    """
    Block-based Normalized Least Mean Squares (NLMS) filter implementation.
    Designed to match specific C7x logic regarding energy calculation and regularization.
    """
    total_samples = len(input_signal)
    if total_samples % block_size != 0:
        raise ValueError("Total samples must be multiple of block_size")
    num_blocks = total_samples // block_size

    dtype = input_signal.dtype.type

    w = np.zeros(filter_length, dtype=dtype)
    x_buffer = np.zeros(filter_length, dtype=dtype)
    write_idx = 0

    # Initialization for type safety
    energy = dtype(0.0)
    regularization = dtype(1e-3)
    output_signal = np.zeros(total_samples, dtype=dtype)

    for block in range(num_blocks):
        block_start = block * block_size
        block_end = block_start + block_size
        accum_update = np.zeros(filter_length, dtype=dtype)

        for n in range(block_start, block_end):
            x_n = dtype(input_signal[n])

            # 1. Update State Buffer
            x_buffer[write_idx] = x_n

            # 2. Brute Force Energy Calculation
            energy = dtype(0.0)
            for k in range(filter_length):
                energy += x_buffer[k] ** 2

            # 3. Compute output y_n (convolution)
            y_n = dtype(0.0)
            idx = write_idx
            for i in range(filter_length):
                y_n += x_buffer[idx] * w[filter_length - 1 - i]
                idx = (idx + 1) % filter_length

            output_signal[n] = y_n
            e_n = dtype(desired_signal[n]) - y_n

            # 4. Normalized step factor
            # Match C7x Logic: Denom = max(Energy, 1e-6) + Reg
            energy_clamped = max(energy, dtype(1e-6))
            step_factor = (dtype(2.0) * dtype(step_size) * e_n) / (
                energy_clamped + regularization
            )

            idx = write_idx
            for i in range(filter_length):
                accum_update[filter_length - 1 - i] += step_factor * x_buffer[idx]
                idx = (idx + 1) % filter_length

            write_idx = (write_idx + 1) % filter_length

        w += accum_update

    return output_signal, w
