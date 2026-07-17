import numpy as np
import matplotlib.pyplot as plt
from scipy import signal
from scipy.fft import fft, fftfreq


def debug_print(message, verbose=False):
    """Print message only if verbose mode is enabled"""
    if verbose:
        print(message)


def generate_spectral_test_signal(fs, num_samples, num_channels, signal_type="sweep"):
    """
    Generate test signals specifically designed for spectral analysis

    Parameters:
    -----------
    fs : int
        Sampling frequency in Hz
    num_samples : int
        Number of samples to generate
    num_channels : int
        Number of channels
    signal_type : str, optional
        Type of test signal to generate:
        - 'sweep': logarithmic sine sweep (default)
        - 'white': white noise
        - 'pink': pink noise
        - 'impulse': impulse response test
        - 'multitone': multiple sine tones

    Returns:
    --------
    numpy.ndarray
        Generated test signal with shape (num_samples, num_channels)
    """
    t = np.arange(num_samples) / fs

    if signal_type == "sweep":
        # Generate logarithmic sine sweep from 20 Hz to fs/2
        signal_out = np.zeros((num_samples, num_channels))
        for ch in range(num_channels):
            # Slightly offset each channel's sweep
            f0 = 20  # Start frequency
            f1 = fs / 2 * 0.95  # End frequency
            sweep = signal.chirp(t, f0, t[-1], f1, method="logarithmic", phi=90 * ch)
            # Apply a window to avoid clicks
            window = np.hanning(num_samples)
            signal_out[:, ch] = sweep * window

    elif signal_type == "white":
        # Generate white noise
        signal_out = np.random.normal(0, 0.1, (num_samples, num_channels))

    elif signal_type == "pink":
        # Generate pink noise (1/f spectrum)
        signal_out = np.zeros((num_samples, num_channels))
        for ch in range(num_channels):
            # Generate white noise
            white = np.random.normal(0, 1, num_samples)
            # Convert to frequency domain
            X = fft(white)
            # Create pink filter (1/f)
            freq = fftfreq(num_samples, 1 / fs)
            freq[0] = freq[1]  # Avoid division by zero
            pink_filter = 1 / np.sqrt(np.abs(freq))
            # Apply filter
            X_pink = X * pink_filter
            # Convert back to time domain
            pink = np.real(np.fft.ifft(X_pink))
            # Normalize
            pink = pink / np.max(np.abs(pink)) * 0.9
            signal_out[:, ch] = pink

    elif signal_type == "impulse":
        # Generate impulse signal
        signal_out = np.zeros((num_samples, num_channels))
        for ch in range(num_channels):
            # Place impulse at different positions for each channel
            pos = num_samples // (2 * (ch + 1))
            signal_out[pos, ch] = 1.0

    elif signal_type == "multitone":
        # Generate multiple sine tones
        signal_out = np.zeros((num_samples, num_channels))
        # Frequencies to test (Hz)
        freqs = [100, 500, 1000, 2000, 5000, 10000]
        for ch in range(num_channels):
            for i, freq in enumerate(freqs):
                # Add sine wave with different phase for each channel
                signal_out[:, ch] += 0.1 * np.sin(2 * np.pi * freq * t + np.pi / 4 * ch)
            # Normalize
            signal_out[:, ch] = (
                signal_out[:, ch] / np.max(np.abs(signal_out[:, ch])) * 0.9
            )

    else:
        raise ValueError(f"Unknown signal type: {signal_type}")

    # Convert to int32 format (Q24)
    signal_int = (signal_out * (2**23)).astype(np.int32)

    return signal_int


def plot_signals(
    input_signal, output_signal, fs, num_channels_in, num_channels_out, save_plots=False
):
    """
    Plot input vs output signals for comparison

    Parameters:
    -----------
    input_signal : numpy.ndarray
        The input signal to the signal chain
    output_signal : numpy.ndarray
        The output signal from the signal chain
    fs : int
        Sampling frequency in Hz
    num_channels_in : int
        Number of input channels
    num_channels_out : int
        Number of output channels
    save_plots : bool, optional
        Whether to save the plots to files (default: False)
    """
    # Create time arrays for x-axis
    num_samples_in = input_signal.shape[0]
    num_samples_out = output_signal.shape[0]
    t_in = np.arange(num_samples_in) / fs
    t_out = np.arange(num_samples_out) / fs

    # Convert input signal to float for better comparison with output
    input_signal_float = input_signal.astype(float)

    # Normalize input and output signals for better comparison
    # Find max absolute values for normalization
    max_abs_in = np.max(np.abs(input_signal_float))
    max_abs_out = np.max(np.abs(output_signal))

    if max_abs_in > 0:
        input_signal_norm = input_signal_float / max_abs_in
    else:
        input_signal_norm = input_signal_float

    if max_abs_out > 0:
        output_signal_norm = output_signal / max_abs_out
    else:
        output_signal_norm = output_signal

    # Create a figure with subplots for each channel comparison
    plt.figure(figsize=(14, 10))

    # For each output channel, create a subplot
    for ch_out in range(num_channels_out):
        plt.subplot(num_channels_out, 1, ch_out + 1)

        # Plot the output channel
        plt.plot(
            t_out, output_signal_norm[:, ch_out], "b-", label=f"Output Ch {ch_out+1}"
        )

        # If this output channel has a corresponding input channel, plot it too
        if ch_out < num_channels_in:
            plt.plot(
                t_in, input_signal_norm[:, ch_out], "r--", label=f"Input Ch {ch_out+1}"
            )
            plt.title(f"Input vs Output - Channel {ch_out+1}")
        else:
            # For output channels without direct input counterparts
            # Find which input channel was routed to this output (using modulo)
            input_ch = ch_out % num_channels_in
            plt.plot(
                t_in,
                input_signal_norm[:, input_ch],
                "r--",
                label=f"Input Ch {input_ch+1}",
            )
            plt.title(f"Input Ch {input_ch+1} vs Output Ch {ch_out+1}")

        plt.xlabel("Time (s)")
        plt.ylabel("Normalized Amplitude")
        plt.grid(True)
        plt.legend()

    plt.tight_layout()

    if save_plots:
        plt.savefig("input_vs_output.png")
    plt.show()

    # Create enhanced frequency domain comparison plot
    plt.figure(figsize=(14, 10))

    # For each output channel, create a subplot for frequency domain
    for ch_out in range(num_channels_out):
        plt.subplot(num_channels_out, 1, ch_out + 1)

        # Compute FFT for output with better frequency resolution
        # Use Welch's method for better spectral estimation
        f_out, psd_out = signal.welch(
            output_signal_norm[:, ch_out],
            fs,
            nperseg=min(256, len(output_signal_norm) // 4),
            scaling="spectrum",
            average="median",
        )

        # Plot the output channel FFT
        plt.plot(
            f_out, 10 * np.log10(psd_out + 1e-10), "b-", label=f"Output Ch {ch_out+1}"
        )

        # If this output channel has a corresponding input channel, plot its FFT too
        if ch_out < num_channels_in:
            f_in, psd_in = signal.welch(
                input_signal_norm[:, ch_out],
                fs,
                nperseg=min(256, len(input_signal_norm) // 4),
                scaling="spectrum",
                average="median",
            )
            plt.plot(
                f_in, 10 * np.log10(psd_in + 1e-10), "r--", label=f"Input Ch {ch_out+1}"
            )
            plt.title(f"Spectral Analysis: Input vs Output - Channel {ch_out+1}")

            # Calculate and display spectral metrics
            # Calculate frequency response (transfer function)
            common_freqs = np.intersect1d(f_in, f_out)
            psd_in_interp = np.interp(common_freqs, f_in, psd_in)
            psd_out_interp = np.interp(common_freqs, f_out, psd_out)

            # Avoid division by zero
            mask = psd_in_interp > 1e-10
            transfer_func = np.zeros_like(common_freqs)
            transfer_func[mask] = 10 * np.log10(
                psd_out_interp[mask] / psd_in_interp[mask]
            )

            # Calculate metrics
            mean_gain = np.mean(transfer_func[mask])
            std_gain = np.std(transfer_func[mask])

            # Add metrics to plot
            plt.text(
                0.02,
                0.95,
                f"Mean Gain: {mean_gain:.2f} dB\nStd Dev: {std_gain:.2f} dB",
                transform=plt.gca().transAxes,
                bbox=dict(facecolor="white", alpha=0.8),
            )

        else:
            # For output channels without direct input counterparts
            input_ch = ch_out % num_channels_in
            f_in, psd_in = signal.welch(
                input_signal_norm[:, input_ch],
                fs,
                nperseg=min(256, len(input_signal_norm) // 4),
                scaling="spectrum",
                average="median",
            )
            plt.plot(
                f_in,
                10 * np.log10(psd_in + 1e-10),
                "r--",
                label=f"Input Ch {input_ch+1}",
            )
            plt.title(
                f"Spectral Analysis: Input Ch {input_ch+1} vs Output Ch {ch_out+1}"
            )

            # Calculate and display spectral metrics
            # Calculate frequency response (transfer function)
            common_freqs = np.intersect1d(f_in, f_out)
            psd_in_interp = np.interp(common_freqs, f_in, psd_in)
            psd_out_interp = np.interp(common_freqs, f_out, psd_out)

            # Avoid division by zero
            mask = psd_in_interp > 1e-10
            transfer_func = np.zeros_like(common_freqs)
            transfer_func[mask] = 10 * np.log10(
                psd_out_interp[mask] / psd_in_interp[mask]
            )

            # Calculate metrics
            mean_gain = np.mean(transfer_func[mask])
            std_gain = np.std(transfer_func[mask])

            # Add metrics to plot
            plt.text(
                0.02,
                0.95,
                f"Mean Gain: {mean_gain:.2f} dB\nStd Dev: {std_gain:.2f} dB",
                transform=plt.gca().transAxes,
                bbox=dict(facecolor="white", alpha=0.8),
            )

        plt.xlabel("Frequency (Hz)")
        plt.ylabel("Power Spectral Density (dB/Hz)")
        plt.grid(True)
        plt.legend()
        plt.xscale("log")
        plt.xlim([20, fs / 2])  # Limit to audible range and Nyquist frequency

        # Add vertical lines at filter cutoff frequencies
        plt.axvline(x=200, color="g", linestyle="--", alpha=0.5, label="Low Shelf")
        plt.axvline(x=1000, color="m", linestyle="--", alpha=0.5, label="Notch")
        plt.axvline(x=5000, color="c", linestyle="--", alpha=0.5, label="High Shelf")

    plt.tight_layout()

    if save_plots:
        plt.savefig("spectral_analysis.png")
    plt.show()

    # Add spectrogram view for time-varying spectral content
    plt.figure(figsize=(14, 10))

    for ch_out in range(
        min(2, num_channels_out)
    ):  # Limit to first 2 channels to save space
        # Input spectrogram
        plt.subplot(2, 2, ch_out * 2 + 1)
        if ch_out < num_channels_in:
            f, t, Sxx = signal.spectrogram(
                input_signal_norm[:, ch_out],
                fs,
                nperseg=min(128, len(input_signal_norm) // 8),
            )
            plt.pcolormesh(t, f, 10 * np.log10(Sxx + 1e-10), shading="gouraud")
            plt.title(f"Input Ch {ch_out+1} Spectrogram")
        else:
            input_ch = ch_out % num_channels_in
            f, t, Sxx = signal.spectrogram(
                input_signal_norm[:, input_ch],
                fs,
                nperseg=min(128, len(input_signal_norm) // 8),
            )
            plt.pcolormesh(t, f, 10 * np.log10(Sxx + 1e-10), shading="gouraud")
            plt.title(f"Input Ch {input_ch+1} Spectrogram")
        plt.ylabel("Frequency (Hz)")
        plt.xlabel("Time (s)")
        plt.colorbar(label="PSD (dB/Hz)")

        # Output spectrogram
        plt.subplot(2, 2, ch_out * 2 + 2)
        f, t, Sxx = signal.spectrogram(
            output_signal_norm[:, ch_out],
            fs,
            nperseg=min(128, len(output_signal_norm) // 8),
        )
        plt.pcolormesh(t, f, 10 * np.log10(Sxx + 1e-10), shading="gouraud")
        plt.title(f"Output Ch {ch_out+1} Spectrogram")
        plt.ylabel("Frequency (Hz)")
        plt.xlabel("Time (s)")
        plt.colorbar(label="PSD (dB/Hz)")

    plt.tight_layout()

    if save_plots:
        plt.savefig("spectrograms.png")
    plt.show()

    # Calculate and plot transfer function (frequency response)
    plt.figure(figsize=(14, 8))

    for ch_out in range(num_channels_out):
        plt.subplot(num_channels_out, 1, ch_out + 1)

        if ch_out < num_channels_in:
            input_ch = ch_out
        else:
            input_ch = ch_out % num_channels_in

        # Calculate transfer function using Welch's method
        f_in, psd_in = signal.welch(
            input_signal_norm[:, input_ch],
            fs,
            nperseg=min(256, len(input_signal_norm) // 4),
        )
        f_out, psd_out = signal.welch(
            output_signal_norm[:, ch_out],
            fs,
            nperseg=min(256, len(output_signal_norm) // 4),
        )

        # Interpolate to common frequency grid
        common_freqs = np.linspace(20, fs / 2, 1000)
        psd_in_interp = np.interp(common_freqs, f_in, psd_in)
        psd_out_interp = np.interp(common_freqs, f_out, psd_out)

        # Calculate transfer function (avoid division by zero)
        mask = psd_in_interp > 1e-10
        transfer_func = np.zeros_like(common_freqs)
        transfer_func[mask] = 10 * np.log10(psd_out_interp[mask] / psd_in_interp[mask])

        # Plot transfer function
        plt.plot(common_freqs[mask], transfer_func[mask])
        plt.title(f"Transfer Function: Input Ch {input_ch+1} → Output Ch {ch_out+1}")
        plt.xlabel("Frequency (Hz)")
        plt.ylabel("Gain (dB)")
        plt.grid(True)
        plt.xscale("log")
        plt.xlim([20, fs / 2])

        # Add vertical lines at filter cutoff frequencies
        plt.axvline(x=200, color="g", linestyle="--", alpha=0.5, label="Low Shelf")
        plt.axvline(x=1000, color="m", linestyle="--", alpha=0.5, label="Notch")
        plt.axvline(x=5000, color="c", linestyle="--", alpha=0.5, label="High Shelf")

        # Add horizontal line at 0 dB
        plt.axhline(y=0, color="k", linestyle="-", alpha=0.3)

        # Add legend
        if ch_out == 0:
            plt.legend()

    plt.tight_layout()

    if save_plots:
        plt.savefig("transfer_function.png")
    plt.show()
