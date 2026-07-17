import numpy as np
import matplotlib.pyplot as plt
from scipy import signal

NUM_COEFS_PER_BIQUAD_FULL = 6


class NChannelCascadeBiquad:
    def __init__(self, fs, filter_cutoff, num_stages, num_channels, btype="low"):
        filter_num_channels = 1

        bCoefs = np.zeros((filter_num_channels, 2 * num_stages + 1))
        aCoefs = np.zeros((filter_num_channels, 2 * num_stages + 1))

        filter_order = 2 * num_stages
        # filterCutoff = LPF_MIN_FREQ + np.random.rand() * LPF_RNG_FREQ
        # design filter coefficients
        [b, a] = signal.butter(filter_order, filter_cutoff / (fs / 2), btype)

        self.sosCoefs = np.zeros((num_stages, NUM_COEFS_PER_BIQUAD_FULL))
        # Design 2nd order sections
        sosChan = signal.tf2sos(b, a)
        self.sosCoefs[:, :] = sosChan
        self.num_stages = num_stages
        self.num_channels = num_channels
        # self.input_signal = input_signal

    def execute(self, input_signal):
        filtered_signal = np.zeros_like(input_signal)
        zi = np.zeros((self.num_stages, 2, self.num_channels))
        zf = np.zeros_like(zi)
        for ch in range(self.num_channels):
            filtered_signal[:, ch], zf[:, :, ch] = signal.sosfilt(
                np.ascontiguousarray(self.sosCoefs[:, :]),
                input_signal[:, ch],
                axis=0,
                zi=zi[:, :, ch],
            )

        return filtered_signal

    def write_coeffs_to_file(self, header_file_name, array_name, mode="a"):
        attributes = '__attribute__((section(".staticData"))) '

        with open(header_file_name, mode) as f:
            f.write(attributes)
            f.write(f"const float {array_name}[] = {{\n")

            numChannels = 1
            numStages = self.sosCoefs.shape[0]
            for dimZ in range(0, numChannels):
                for dimX in range(0, numStages):
                    coeffs = self.sosCoefs[
                        dimX, [0, 1, 2, 4, 5]
                    ]  # Exclude a0=1.0 coefficient at index 3
                    for coeff in coeffs:
                        print("{:>2}".format(coeff), file=f, end=", ")
                    f.write("\n\n")
            f.write("\n};")
            f.write("\n\n")
            f.close()
