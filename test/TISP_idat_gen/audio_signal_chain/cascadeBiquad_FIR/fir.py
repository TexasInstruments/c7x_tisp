import numpy as np
import matplotlib.pyplot as plt
from scipy import signal


class NChannelFIRFilter:
    def __init__(self, fs, cutoff, num_taps, num_channels):
        """
        Initializes the n-channel FIR filter.

        Args:
            fs (int): Sampling frequency.
            cutoff (int): Cutoff frequency.
            num_taps (int): Number of taps.
            num_channels (int): Number of channels.
        """
        self.fs = fs
        self.cutoff = cutoff
        self.num_taps = num_taps
        self.num_channels = num_channels
        self.coefficients = self.calculate_coefficients()

    def calculate_coefficients(self):
        """
        Calculates the FIR filter coefficients.

        Returns:
            np.ndarray: Filter coefficients.
        """
        normalized_cutoff = self.cutoff / (self.fs / 2)
        coefficients = signal.firwin(self.num_taps, normalized_cutoff, pass_zero=True)
        # print("Filter gain:", np.sum(coefficients))  # Print the filter gain
        return coefficients

    def execute(self, input_signal):
        """
        Applies the n-channel FIR filter to the input signal.

        Args:
            input_signal (np.ndarray): Input signal with shape (num_channels, num_samples).

        Returns:
            np.ndarray: Filtered signal with shape (num_channels, num_samples).
        """
        input_size = input_signal.shape[1]
        filter_size = self.num_taps
        state_data = np.zeros(
            (self.num_channels, (input_size + filter_size - 1))
        ).astype("float")

        state_data[:, filter_size - 1 :] = input_signal

        filtered_signal = np.zeros((self.num_channels, input_size))

        for i in range(self.num_channels):
            filtered_signal[i, :] = np.convolve(
                state_data[i, :], self.coefficients, mode="vaild"
            )
        return filtered_signal

    def plot_frequency_response(self):
        """
        Plots the frequency response of the FIR filter.
        """
        w, h = signal.freqz(self.coefficients, fs=self.fs)
        plt.figure(figsize=(8, 6))
        plt.plot(w, np.abs(h))
        plt.xlabel("Frequency (Hz)")
        plt.ylabel("Magnitude")
        plt.title("Magnitude Response of the FIR Filter")
        plt.grid(True)
        plt.show()


# # Example usage:
# fs = 48000
# cutoff = 3000
# num_taps = 64
# num_channels = 8
# num_samples = 256

# input_signal = np.random.rand(num_samples, num_channels)

# fir_filter = NChannelFIRFilter(fs, cutoff, num_taps, num_channels)
# fir_filtered_signal = fir_filter.execute(input_signal)
# # fir_filter.plot_frequency_response()

# # Plot the original and filtered signals
# # plt.figure(figsize=(8, 6))
# # for i in range(num_channels):
# #     plt.plot(input_signal[:, i], label=f"Original Channel {i+1}")
# #     plt.plot(filtered_signal[:, i], label=f"Filtered Channel {i+1}")
# # plt.xlabel("Time")
# # plt.ylabel("Amplitude")
# # plt.title("Original and Filtered Signals")
# # plt.legend()
# # plt.grid(True)
# # plt.show()
