from plot_spectrum import *
import sys

file_type = str(sys.argv[4])
num_channels = int(sys.argv[5])
num_samples = int(sys.argv[6])


if file_type == "txt":
    plot_audio_spectrum_from_file(
        sys.argv[1], sys.argv[2], num_channels, num_samples, int(sys.argv[3])
    )

if file_type == "wav":
    plot_audio_spectrum_from_wav_file(
        sys.argv[1], sys.argv[2], num_channels, num_samples, int(sys.argv[3])
    )
