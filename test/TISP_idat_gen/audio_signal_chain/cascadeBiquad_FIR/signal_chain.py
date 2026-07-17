import numpy as np
import soundfile as sf
from write_wav_to_txt import *

import matplotlib.pyplot as plt

import subprocess


import sys

sys.path.insert(0, "../../")

from common.TISP_file_io import TensorToCppHeader
from fir import NChannelFIRFilter
from cascadeBiquad import NChannelCascadeBiquad
from plot_spectrum import plot_audio_spectrum

# Cascadebiquad
fs = 48000
cutoff_fir = 3000  # lpf1
cutoff_cascade0 = 5000  # lpf0
cutoff_cascade1 = 500  # hpf0
num_stages = 3
num_channels = 8
num_taps = 64
num_samples = int(4096 * 1e0)
# num_samples = int(256)
# input_signal = np.random.uniform(
#     low=-1, high=1, size=(num_samples, num_channels)
# ).astype("float")

input_signal = np.round(
    np.random.uniform(
        low=-(2**12), high=2**12, size=(num_samples, num_channels)
    ).astype("short"),
    3,
)


# print(input_signal)

fir_filter = NChannelFIRFilter(fs, cutoff_fir, num_taps, num_channels)
cascade_filter0 = NChannelCascadeBiquad(fs, cutoff_cascade0, num_stages, num_channels)
cascade_filter1 = NChannelCascadeBiquad(
    fs, cutoff_cascade1, num_stages, num_channels, "high"
)


# filtered_signal = np.transpose(input_signal)
filtered_signal = np.round(cascade_filter0.execute(input_signal), 3)
filtered_signal = np.round(cascade_filter1.execute(filtered_signal), 3)
filtered_signal = np.round(np.transpose(filtered_signal), 3)
filtered_signal = np.round(fir_filter.execute((filtered_signal)), 3)
filtered_signal = np.round(np.transpose(filtered_signal), 3)

# Extract the last column to mimic router pConfigRouter = {0,1,2,3,4,5,6,7,8,0,1,2,3}
last_columns = filtered_signal[:, 0:4]

# Replicate the last column n times
# replicated_columns = np.tile(last_column, (1, 4))
# filtered_signal = np.concatenate((filtered_signal, replicated_columns), axis=1)
filtered_signal = np.concatenate((filtered_signal, last_columns), axis=1)


# Create a TensorToCppHeader object
test_case = str(5)
header_file_name = (
    "../../../audio_signal_chain/TISP_cascadeBiquad_FIR/test_data/staticCase"
    + test_case
    + ".hpp"
)
tensor_to_cpp_header = TensorToCppHeader(header_file_name, mode="new")


# Write the input signal and filtered signal to the header file

tensor_to_cpp_header.write_tensor_to_header(
    input_signal, "inputSignalCase" + test_case, "int32_t"
)

tensor_to_cpp_header.write_tensor_to_header(
    fir_filter.coefficients, "filterCoeffCase" + test_case, "float"
)
cascade_filter0.write_coeffs_to_file(
    header_file_name, "cascadeBiquad0CoeffCase" + test_case
)
cascade_filter1.write_coeffs_to_file(
    header_file_name, "cascadeBiquad1CoeffCase" + test_case
)

tensor_to_cpp_header.write_tensor_to_header(
    ((np.round(filtered_signal))).astype(np.int16),
    "outputSignalCase" + test_case,
    "int32_t",
)


plot_audio_spectrum(np.transpose(input_signal), np.transpose(filtered_signal), 0)


sf.write("input.wav", input_signal, fs, subtype="PCM_32")
sf.write(
    "output.wav", (np.round(filtered_signal)).astype(np.int16), fs, subtype="PCM_32"
)


# data, fs = sf.read("input.wav", dtype="int16")

# print(data[:, 0])
# print(input_signal[:, 0])
# plt.plot(data[:, 0])
# plt.plot(input_signal[:, 0])
# plt.show()

write_wav_to_txt("input.wav", "input_ch0.txt", 0)
write_wav_to_txt("output.wav", "out_ch0.txt", 0)


scp_flag = sys.argv[1]

if int(scp_flag) == 1:
    scp_command = "scp input.wav am62d-evm:/usr/share/sample_audio.wav"

    # Run the SCP command
    subprocess.run(scp_command, shell=True)
