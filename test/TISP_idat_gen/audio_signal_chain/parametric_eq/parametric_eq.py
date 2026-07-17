import numpy as np
import matplotlib.pyplot as plt
from scipy import signal
import os

import sys

sys.path.insert(0, "../../")

from common.TISP_file_io import TensorToCppHeader


class ParametricEQ:
    """
    A class implementing parametric equalization using a 3-stage cascade biquad filter:
    - Low-shelf filter
    - Notch filter
    - High-shelf filter

    This class demonstrates how to design and apply these filters to shape the frequency
    spectrum of an audio signal between 20Hz and 10kHz with a sampling rate of 48kHz.

    The implementation supports multichannel audio in channel interleaved form where
    the data is organized as x[samples, channels].
    """

    def __init__(self, fs=48000, num_channels=1):
        """
        Initialize the parametric equalizer.

        Parameters:
        -----------
        fs : int
            Sampling frequency in Hz (default: 48000)
        num_channels : int
            Number of audio channels (default: 1)
        """
        self.fs = fs
        self.num_channels = num_channels
        self.sos_coeffs = None

    def design_low_shelf(self, fc, gain_db, Q=0.7071):
        """
        Design a low-shelf filter.

        Parameters:
        -----------
        fc : float
            Cutoff frequency in Hz
        gain_db : float
            Gain in dB
        Q : float
            Quality factor (default: 0.7071, which is 1/sqrt(2))

        Returns:
        --------
        sos : ndarray
            Second-order sections representation of the filter
        """
        # Convert gain from dB to linear
        A = 10 ** (gain_db / 40)

        # Compute filter coefficients
        w0 = 2 * np.pi * fc / self.fs
        alpha = np.sin(w0) / (2 * Q)

        # Compute shelf filter coefficients
        if gain_db >= 0:  # Boost
            b0 = A * ((A + 1) + (A - 1) * np.cos(w0) + 2 * np.sqrt(A) * alpha)
            b1 = -2 * A * ((A - 1) + (A + 1) * np.cos(w0))
            b2 = A * ((A + 1) + (A - 1) * np.cos(w0) - 2 * np.sqrt(A) * alpha)
            a0 = (A + 1) - (A - 1) * np.cos(w0) + 2 * np.sqrt(A) * alpha
            a1 = 2 * ((A - 1) - (A + 1) * np.cos(w0))
            a2 = (A + 1) - (A - 1) * np.cos(w0) - 2 * np.sqrt(A) * alpha
        else:  # Cut
            b0 = (A + 1) - (A - 1) * np.cos(w0) + 2 * np.sqrt(A) * alpha
            b1 = 2 * ((A - 1) - (A + 1) * np.cos(w0))
            b2 = (A + 1) - (A - 1) * np.cos(w0) - 2 * np.sqrt(A) * alpha
            a0 = A * ((A + 1) + (A - 1) * np.cos(w0) + 2 * np.sqrt(A) * alpha)
            a1 = -2 * A * ((A - 1) + (A + 1) * np.cos(w0))
            a2 = A * ((A + 1) + (A - 1) * np.cos(w0) - 2 * np.sqrt(A) * alpha)

        # Normalize by a0
        b = np.array([b0, b1, b2]) / a0
        a = np.array([1.0, a1 / a0, a2 / a0])

        # Convert to SOS format
        sos = np.array([[b[0], b[1], b[2], 1.0, a[1], a[2]]])

        return sos

    def design_notch(self, fc, Q):
        """
        Design a notch filter.

        Parameters:
        -----------
        fc : float
            Center frequency in Hz
        Q : float
            Quality factor (controls bandwidth)

        Returns:
        --------
        sos : ndarray
            Second-order sections representation of the filter
        """
        # Compute normalized frequency
        w0 = 2 * np.pi * fc / self.fs

        # Compute filter coefficients
        alpha = np.sin(w0) / (2 * Q)

        b0 = 1.0
        b1 = -2.0 * np.cos(w0)
        b2 = 1.0
        a0 = 1.0 + alpha
        a1 = -2.0 * np.cos(w0)
        a2 = 1.0 - alpha

        # Normalize by a0
        b = np.array([b0, b1, b2]) / a0
        a = np.array([1.0, a1 / a0, a2 / a0])

        # Convert to SOS format
        sos = np.array([[b[0], b[1], b[2], 1.0, a[1], a[2]]])

        return sos

    def design_high_shelf(self, fc, gain_db, Q=0.7071):
        """
        Design a high-shelf filter.

        Parameters:
        -----------
        fc : float
            Cutoff frequency in Hz
        gain_db : float
            Gain in dB
        Q : float
            Quality factor (default: 0.7071, which is 1/sqrt(2))

        Returns:
        --------
        sos : ndarray
            Second-order sections representation of the filter
        """
        # Convert gain from dB to linear
        A = 10 ** (gain_db / 40)

        # Compute filter coefficients
        w0 = 2 * np.pi * fc / self.fs
        alpha = np.sin(w0) / (2 * Q)

        # Compute shelf filter coefficients
        if gain_db >= 0:  # Boost
            b0 = A * ((A + 1) - (A - 1) * np.cos(w0) + 2 * np.sqrt(A) * alpha)
            b1 = 2 * A * ((A - 1) - (A + 1) * np.cos(w0))
            b2 = A * ((A + 1) - (A - 1) * np.cos(w0) - 2 * np.sqrt(A) * alpha)
            a0 = (A + 1) + (A - 1) * np.cos(w0) + 2 * np.sqrt(A) * alpha
            a1 = -2 * ((A - 1) + (A + 1) * np.cos(w0))
            a2 = (A + 1) + (A - 1) * np.cos(w0) - 2 * np.sqrt(A) * alpha
        else:  # Cut
            b0 = (A + 1) + (A - 1) * np.cos(w0) + 2 * np.sqrt(A) * alpha
            b1 = -2 * ((A - 1) + (A + 1) * np.cos(w0))
            b2 = (A + 1) + (A - 1) * np.cos(w0) - 2 * np.sqrt(A) * alpha
            a0 = A * ((A + 1) - (A - 1) * np.cos(w0) + 2 * np.sqrt(A) * alpha)
            a1 = 2 * A * ((A - 1) - (A + 1) * np.cos(w0))
            a2 = A * ((A + 1) - (A - 1) * np.cos(w0) - 2 * np.sqrt(A) * alpha)

        # Normalize by a0
        b = np.array([b0, b1, b2]) / a0
        a = np.array([1.0, a1 / a0, a2 / a0])

        # Convert to SOS format
        sos = np.array([[b[0], b[1], b[2], 1.0, a[1], a[2]]])

        return sos

    def design_parametric_eq(self, low_shelf_params, notch_params, high_shelf_params):
        """
        Design a 3-stage parametric EQ with low-shelf, notch, and high-shelf filters.

        Parameters:
        -----------
        low_shelf_params : tuple
            (cutoff_freq, gain_db, Q) for the low-shelf filter
        notch_params : tuple
            (center_freq, Q) for the notch filter
        high_shelf_params : tuple
            (cutoff_freq, gain_db, Q) for the high-shelf filter

        Returns:
        --------
        sos : ndarray
            Second-order sections representation of the cascaded filters
        """
        # Design individual filters
        low_shelf_sos = self.design_low_shelf(*low_shelf_params)
        notch_sos = self.design_notch(*notch_params)
        high_shelf_sos = self.design_high_shelf(*high_shelf_params)

        # Cascade the filters
        self.sos_coeffs = np.vstack((low_shelf_sos, notch_sos, high_shelf_sos))

        return self.sos_coeffs

    def apply_filter(self, x):
        """
        Apply the designed parametric EQ to an input signal.

        Parameters:
        -----------
        x : ndarray
            Input signal. For multichannel audio, the shape should be (samples, channels)
            with samples as the first dimension and channels as the second dimension.

        Returns:
        --------
        y : ndarray
            Filtered output signal in the same format as the input
        """
        if self.sos_coeffs is None:
            raise ValueError(
                "Filter coefficients not designed. Call design_parametric_eq first."
            )

        # Initialize output array
        if len(x.shape) == 1:
            # Single channel case
            y = signal.sosfilt(self.sos_coeffs, x)
        else:
            # Multichannel case
            y = np.zeros_like(x)

            # Process each channel separately
            for ch in range(self.num_channels):
                y[:, ch] = signal.sosfilt(self.sos_coeffs, x[:, ch])

        return y

    def plot_frequency_response(
        self,
        low_shelf_params,
        notch_params,
        high_shelf_params,
        fmin=20,
        fmax=10000,
        num_points=1000,
        enable_plotting=True,
    ):
        """
        Plot the frequency response of the parametric EQ and its individual filters.

        Parameters:
        -----------
        low_shelf_params : tuple
            (cutoff_freq, gain_db, Q) for the low-shelf filter
        notch_params : tuple
            (center_freq, Q) for the notch filter
        high_shelf_params : tuple
            (cutoff_freq, gain_db, Q) for the high-shelf filter
        fmin : float
            Minimum frequency for the plot in Hz (default: 20)
        fmax : float
            Maximum frequency for the plot in Hz (default: 10000)
        num_points : int
            Number of frequency points to evaluate (default: 1000)
        enable_plotting : bool
            Whether to display plots (default: True)

        Returns:
        --------
        dict
            Dictionary containing frequency response data if enable_plotting is False:
            - 'f': frequency points
            - 'h_low_db': low-shelf filter response in dB
            - 'h_notch_db': notch filter response in dB
            - 'h_high_db': high-shelf filter response in dB
            - 'h_combined_db': combined filter response in dB
        """
        # Design individual filters
        low_shelf_sos = self.design_low_shelf(*low_shelf_params)
        notch_sos = self.design_notch(*notch_params)
        high_shelf_sos = self.design_high_shelf(*high_shelf_params)

        # Design combined filter
        combined_sos = np.vstack((low_shelf_sos, notch_sos, high_shelf_sos))

        # Compute frequency responses
        f = np.logspace(np.log10(fmin), np.log10(fmax), num_points)
        w = 2 * np.pi * f / self.fs

        _, h_low = signal.sosfreqz(low_shelf_sos, worN=w)
        _, h_notch = signal.sosfreqz(notch_sos, worN=w)
        _, h_high = signal.sosfreqz(high_shelf_sos, worN=w)
        _, h_combined = signal.sosfreqz(combined_sos, worN=w)

        # Convert to dB
        h_low_db = 20 * np.log10(np.abs(h_low))
        h_notch_db = 20 * np.log10(np.abs(h_notch))
        h_high_db = 20 * np.log10(np.abs(h_high))
        h_combined_db = 20 * np.log10(np.abs(h_combined))

        # Create a dictionary with the frequency response data
        freq_response_data = {
            "f": f,
            "h_low_db": h_low_db,
            "h_notch_db": h_notch_db,
            "h_high_db": h_high_db,
            "h_combined_db": h_combined_db,
        }

        # Plot if enabled
        if enable_plotting:
            plt.figure(figsize=(12, 8))

            plt.semilogx(f, h_low_db, label="Low-shelf")
            plt.semilogx(f, h_notch_db, label="Notch")
            plt.semilogx(f, h_high_db, label="High-shelf")
            plt.semilogx(f, h_combined_db, "k--", linewidth=2, label="Combined")

            plt.grid(True, which="both", ls="--", alpha=0.7)
            plt.xlabel("Frequency (Hz)")
            plt.ylabel("Magnitude (dB)")
            plt.title("Parametric EQ Frequency Response")
            plt.legend()
            plt.xlim(fmin, fmax)

            # Add vertical lines at filter cutoff/center frequencies
            plt.axvline(x=low_shelf_params[0], color="b", linestyle=":", alpha=0.7)
            plt.axvline(x=notch_params[0], color="g", linestyle=":", alpha=0.7)
            plt.axvline(x=high_shelf_params[0], color="r", linestyle=":", alpha=0.7)

            plt.tight_layout()
            plt.show()

        return freq_response_data

    def write_coeffs_to_file(self, header_file_name, array_name, mode="a"):
        attributes = '__attribute__((section(".staticData"))) '

        with open(header_file_name, mode) as f:
            f.write(attributes)
            f.write(f"const float {array_name}[] = {{\n")

            # same coeff for all channels
            numChannels = 1
            numStages = self.sos_coeffs.shape[0]
            for dimZ in range(0, numChannels):
                for dimX in range(0, numStages):
                    coeffs = self.sos_coeffs[
                        dimX, [0, 1, 2, 4, 5]
                    ]  # Exclude a0=1.0 coefficient at index 3
                    for coeff in coeffs:
                        print("{:>2}".format(coeff), file=f, end=", ")
                    f.write("\n\n")
            f.write("\n};")
            f.write("\n\n")
            f.close()


def generate_test_signal(fs, duration=4.0, num_channels=1, effective_bits=24):
    """
    Generate a uniform distribution test signal simulating ADC data in Q24 format with specified effective bits.

    Parameters:
    -----------
    fs : int
        Sampling frequency in Hz
    duration : float
        Duration of the signal in seconds (default: 4.0)
    num_channels : int
        Number of audio channels (default: 1)
    effective_bits : int
        Effective bit depth to limit the signal to (default: 29)

    Returns:
    --------
    t : ndarray
        Time vector
    x : ndarray
        Simulated ADC data in Q24 format. For multichannel audio, the shape will be (samples, channels)
        with samples as the first dimension and channels as the second dimension.
    """
    # Create time vector
    t = np.linspace(0, duration, int(fs * duration), endpoint=False)

    # Q24 format has a range of [-2^23, 2^23-1]
    q24_max = 2**23 - 1
    q24_min = -(2**23)

    # Calculate the effective range for the limited bit depth
    effective_max = 2 ** (effective_bits - 1) - 1
    effective_min = -(2 ** (effective_bits - 1))

    if num_channels == 1:
        # Generate uniform distribution signal
        x_float = np.random.uniform(-1, 1, len(t))

        # Scale to effective bit range
        x_float = x_float * effective_max

        # Convert to integer and limit to effective bit range
        x = np.clip(np.round(x_float), effective_min, effective_max).astype(np.int32)
    else:
        # Generate multichannel uniform distribution signal
        x = np.zeros((len(t), num_channels), dtype=np.int32)

        # Generate uniform distribution for each channel
        for ch in range(num_channels):
            # Generate uniform distribution
            channel_signal = np.random.uniform(-1, 1, len(t))

            # Scale to effective bit range
            channel_signal = channel_signal * effective_max

            # Convert to integer and limit to effective bit range
            x[:, ch] = np.clip(
                np.round(channel_signal), effective_min, effective_max
            ).astype(np.int32)

    return t, x


def normalize_q24_data(x):
    """
    Normalize Q24 data to the range [-1.0, 1.0] for processing with the parametric EQ.

    Parameters:
    -----------
    x : ndarray
        ADC data in Q24 format (int32)

    Returns:
    --------
    x_norm : ndarray
        Normalized data in the range [-1.0, 1.0]
    """
    # Q24 format has a range of [-2^23, 2^23-1]
    q24_max = 2**23 - 1

    # Normalize to [-1.0, 1.0]
    x_norm = x.astype(np.float32) / q24_max

    return x_norm


def denormalize_to_q24(x_norm, effective_bits=23):
    """
    Denormalize data from the range [-1.0, 1.0] back to Q24 format.

    Parameters:
    -----------
    x_norm : ndarray
        Normalized data in the range [-1.0, 1.0]
    effective_bits : int
        Effective bit depth to limit the output to (default: 23)

    Returns:
    --------
    x : ndarray
        Denormalized data in Q24 format (int32)
    """
    # Q24 format has a range of [-2^23, 2^23-1]
    q24_max = 2**23 - 1
    q24_min = -(2**23)

    # Calculate the effective range for the limited bit depth
    effective_max = 2 ** (effective_bits - 1) - 1
    effective_min = -(2 ** (effective_bits - 1))

    # Scale back to Q24 range and clip to effective bit range
    # x = np.clip(np.round(x_norm * q24_max), effective_min, effective_max).astype(
    #     np.int32
    # )

    x = np.round(x_norm * q24_max).astype(np.int32)

    return x


def write_output_to_text_file(y_q24, filename):
    """
    Write the reference output (y_q24) to a text file in interleaved form.

    Parameters:
    -----------
    y_q24 : ndarray
        Output signal in Q24 format. For multichannel audio, the shape should be (samples, channels)
    filename : str
        Path to the output text file

    Notes:
    ------
    The data is stored in interleaved form where the first n samples correspond to
    the sample 0 value of n channels, followed by sample 1 values, and so on.
    """
    # Create directory if it doesn't exist
    # os.makedirs(os.path.dirname(filename), exist_ok=True)

    # Get the number of samples and channels
    num_samples, num_channels = y_q24.shape

    # Open the file for writing
    with open(filename, "w") as f:
        # Write interleaved data
        for sample_idx in range(num_samples):
            for channel_idx in range(num_channels):
                f.write(f"{y_q24[sample_idx, channel_idx]}\n")

    print(f"Reference output written to {filename}")


def plot_spectrum(
    x, fs, title="Spectrum", channel=0, fmin=20, fmax=10000, enable_plotting=True
):
    """
    Plot the frequency spectrum of a signal using FFT.

    Parameters:
    -----------
    x : ndarray
        Signal. For multichannel audio, the shape should be (samples, channels)
        with samples as the first dimension and channels as the second dimension.
    fs : int
        Sampling frequency in Hz
    title : str
        Title for the plot (default: "Spectrum")
    channel : int
        Channel to plot for multichannel audio (default: 0)
    fmin : float
        Minimum frequency for the plot in Hz (default: 20)
    fmax : float
        Maximum frequency for the plot in Hz (default: 10000)
    enable_plotting : bool
        Whether to display plots (default: True)

    Returns:
    --------
    dict
        Dictionary containing spectrum data if enable_plotting is False:
        - 'f': frequency bins
        - 'X_db': magnitude spectrum in dB
    """
    # Extract the channel to plot
    if len(x.shape) > 1 and x.shape[1] > 1:
        # Extract the specified channel from multichannel data
        x_channel = x[:, channel]
    else:
        x_channel = x

    # Compute the FFT
    n = len(x_channel)
    X = np.fft.rfft(x_channel)
    X_mag = np.abs(X) / n  # Normalize

    # Compute the frequency bins
    f = np.fft.rfftfreq(n, d=1 / fs)

    # Convert to dB
    X_db = 20 * np.log10(X_mag + 1e-10)  # Add small value to avoid log(0)

    # Create a dictionary with the spectrum data
    spectrum_data = {"f": f, "X_db": X_db}

    # Plot if enabled
    if enable_plotting:
        plt.figure(figsize=(10, 6))

        # Plot only up to fmax
        mask = (f >= fmin) & (f <= fmax)
        plt.semilogx(f[mask], X_db[mask])

        plt.grid(True, which="both", ls="--", alpha=0.7)
        plt.xlabel("Frequency (Hz)")
        plt.ylabel("Magnitude (dB)")

        # Add channel information to title for multichannel audio
        if len(x.shape) > 1 and x.shape[1] > 1:
            title = f"{title} - Channel {channel}"

        plt.title(title)
        plt.xlim(fmin, fmax)

        plt.tight_layout()
        plt.show()

    return spectrum_data


def main(enable_plotting=True):
    """
    Main function demonstrating parametric equalization with simulated Q24 ADC data
    limited to 29 bits of effective resolution, using uniform distribution input.

    Parameters:
    -----------
    enable_plotting : bool
        Whether to display plots (default: True)

    Returns:
    --------
    dict
        Dictionary containing the processed data and results
    """
    # Set parameters
    fs = 48000  # Sampling frequency in Hz
    num_channels = 8  # Number of audio channels
    num_samples = 4096
    effective_bits = 20  # Effective bit depth to limit the signal to

    # Create parametric EQ
    eq = ParametricEQ(fs=fs, num_channels=num_channels)

    # Define filter parameters
    # Low-shelf: boost bass frequencies below 200 Hz by 6 dB
    low_shelf_params = (200, 6, 0.7071)

    # Notch: cut frequencies around 1 kHz with a narrow Q
    notch_params = (1000, 5.0)

    # High-shelf: cut high frequencies above 5 kHz by 3 dB
    high_shelf_params = (5000, -3, 0.7071)

    # Design the parametric EQ
    sos_coeffs = eq.design_parametric_eq(
        low_shelf_params, notch_params, high_shelf_params
    )

    # Generate a simulated Q24 ADC test signal
    t, x_q24 = generate_test_signal(
        fs,
        duration=num_samples / fs,
        num_channels=num_channels,
        effective_bits=effective_bits,
    )

    # Normalize the Q24 data for processing
    x_norm = normalize_q24_data(x_q24)

    # x_norm = np.random.uniform(
    #     low=-0.25, high=0.25, size=(num_samples, num_channels)
    # ).astype("float32")

    x_q24 = denormalize_to_q24(x_norm, effective_bits=effective_bits)

    # Apply the parametric EQ to the normalized data
    y_norm = eq.apply_filter(x_norm)

    # Denormalize the processed data back to Q24 format
    y_q24 = denormalize_to_q24(y_norm, effective_bits=effective_bits)

    # Plot the theoretical frequency response of the filter
    freq_response = eq.plot_frequency_response(
        low_shelf_params,
        notch_params,
        high_shelf_params,
        enable_plotting=enable_plotting,
    )

    # Plot the spectrum of the original and filtered signals for channel 0 only
    input_spectrum = plot_spectrum(
        x_norm,
        fs,
        title="Original Signal Spectrum (Normalized)",
        channel=0,
        enable_plotting=enable_plotting,
    )
    output_spectrum = plot_spectrum(
        y_norm,
        fs,
        title="Filtered Signal Spectrum (Normalized)",
        channel=0,
        enable_plotting=enable_plotting,
    )

    # Write data to header file
    test_case = str(1)
    header_file_name = (
        "../../../audio_signal_chain/TISP_parametericEq/test_data/staticCase"
        + test_case
        + ".hpp"
    )

    tensor_to_cpp_header = TensorToCppHeader(header_file_name, mode="new")
    tensor_to_cpp_header.write_tensor_to_header(
        x_q24, "inputSignalCase" + test_case, "int32_t"
    )
    eq.write_coeffs_to_file(header_file_name, "cascadeBiquad0CoeffCase" + test_case)
    tensor_to_cpp_header.write_tensor_to_header(
        y_norm, "cascadeBiquadOutCase" + test_case, "float"
    )

    tensor_to_cpp_header.write_tensor_to_header(
        y_q24, "outputSignalCase" + test_case, "int32_t"
    )

    # Write reference output to text file
    output_filename = f"reference_output_case{test_case}.txt"
    write_output_to_text_file(y_q24, output_filename)

    # Plot the time domain signals for channel 0 only if plotting is enabled
    if enable_plotting:
        plt.figure(figsize=(12, 8))

        # Plot original Q24 data
        plt.subplot(2, 2, 1)
        plt.plot(t, x_q24[:, 0])
        plt.title("Original Q24 Signal - Channel 0")
        plt.xlabel("Time (s)")
        plt.ylabel("Amplitude (Q24)")
        plt.grid(True)

        # Plot normalized data
        plt.subplot(2, 2, 2)
        plt.plot(t, x_norm[:, 0])
        plt.title("Normalized Signal - Channel 0")
        plt.xlabel("Time (s)")
        plt.ylabel("Amplitude (normalized)")
        plt.grid(True)

        # Plot filtered normalized data
        plt.subplot(2, 2, 3)
        plt.plot(t, y_norm[:, 0])
        plt.title("Filtered Normalized Signal - Channel 0")
        plt.xlabel("Time (s)")
        plt.ylabel("Amplitude (normalized)")
        plt.grid(True)

        # Plot filtered Q24 data
        plt.subplot(2, 2, 4)
        plt.plot(t, y_q24[:, 0])
        plt.title("Filtered Q24 Signal - Channel 0")
        plt.xlabel("Time (s)")
        plt.ylabel("Amplitude (Q24)")
        plt.grid(True)

        plt.tight_layout()
        plt.show()

        # Plot histograms to show the distribution of values
        plt.figure(figsize=(12, 8))

        plt.subplot(2, 2, 1)
        plt.hist(x_q24[:, 0], bins=50)
        plt.title("Original Q24 Signal Histogram")
        plt.xlabel("Q24 Value")
        plt.ylabel("Count")

        plt.subplot(2, 2, 2)
        plt.hist(x_norm[:, 0], bins=50)
        plt.title("Normalized Signal Histogram")
        plt.xlabel("Normalized Value")
        plt.ylabel("Count")

        plt.subplot(2, 2, 3)
        plt.hist(y_norm[:, 0], bins=50)
        plt.title("Filtered Normalized Signal Histogram")
        plt.xlabel("Normalized Value")
        plt.ylabel("Count")

        plt.subplot(2, 2, 4)
        plt.hist(y_q24[:, 0], bins=50)
        plt.title("Filtered Q24 Signal Histogram")
        plt.xlabel("Q24 Value")
        plt.ylabel("Count")

        plt.tight_layout()
        plt.show()

    # Print information about the signal shapes and ranges
    print(f"Input Q24 signal shape: {x_q24.shape}")
    print(f"Input Q24 signal range: [{np.min(x_q24)}, {np.max(x_q24)}]")
    print(f"Normalized input signal range: [{np.min(x_norm)}, {np.max(x_norm)}]")
    print(f"Normalized output signal range: [{np.min(y_norm)}, {np.max(y_norm)}]")
    print(f"Output Q24 signal range: [{np.min(y_q24)}, {np.max(y_q24)}]")

    # Calculate the effective number of bits used
    q24_max = 2**23 - 1
    input_ratio = np.max(np.abs(x_q24)) / q24_max
    output_ratio = np.max(np.abs(y_q24)) / q24_max
    input_enob = 24 + np.log2(input_ratio)
    output_enob = 24 + np.log2(output_ratio)

    print(f"Input effective number of bits: {input_enob:.2f}")
    print(f"Output effective number of bits: {output_enob:.2f}")

    # Show Q24 format details
    print("\nQ24 Format Details:")
    print(f"Full range: [{-2**23}, {2**23-1}]")
    print(
        f"Effective range used: [{-2**(effective_bits-1)}, {2**(effective_bits-1)-1}]"
    )
    print(f"Normalized range: [-1.0, 1.0)")

    # Return a dictionary with all the data and results
    results = {
        "time": t,
        "x_q24": x_q24,
        "x_norm": x_norm,
        "y_norm": y_norm,
        "y_q24": y_q24,
        "freq_response": freq_response,
        "input_spectrum": input_spectrum,
        "output_spectrum": output_spectrum,
        "filter_params": {
            "low_shelf": low_shelf_params,
            "notch": notch_params,
            "high_shelf": high_shelf_params,
        },
        "stats": {
            "input_range": [np.min(x_q24), np.max(x_q24)],
            "output_range": [np.min(y_q24), np.max(y_q24)],
            "input_enob": input_enob,
            "output_enob": output_enob,
        },
    }

    return results


if __name__ == "__main__":
    import argparse

    # Parse command line arguments
    parser = argparse.ArgumentParser(description="Parametric EQ demonstration")
    parser.add_argument("--no-plot", action="store_true", help="Disable plotting")
    args = parser.parse_args()

    # Run the main function with plotting enabled or disabled based on the command line argument
    results = main(enable_plotting=not args.no_plot)
