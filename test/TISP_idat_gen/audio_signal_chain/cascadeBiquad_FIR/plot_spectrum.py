import numpy as np
import matplotlib.pyplot as plt
import soundfile as sf


def plot_audio_spectrum(input_signal, filtered_signal, channel=0, fs=48e3):

    x = input_signal[channel, :]
    X = np.fft.fft(x)
    y = filtered_signal[channel, :]
    Y = np.fft.fft(y)
    freq = np.fft.fftfreq(len(x), 1 / fs)

    # print(input_signal[channel, -10:])
    # print(filtered_signal[channel, -10:])

    # Plot the spectra, limited to positive frequencies
    mask = freq >= 0
    plt.plot(freq[mask], np.abs(X[mask]))
    plt.plot(freq[mask], np.abs(Y[mask]))

    plt.xlabel("Frequency (Hz)")
    plt.ylabel("Magnitude")
    plt.title(f"Channel {channel}: Spectrum of Input and Output Signals")
    plt.legend(["Input Signal", "Filtered Output Signal"])
    plt.show()


def plot_audio_spectrum_wav(wav, channel=0, fs=48e3):

    data, fs = sf.read(wav)

    data = data[:, channel]
    freq = np.fft.fftfreq(len(data), 1 / fs)
    X = np.fft.fft(data)
    # print(input_signal[channel, -10:])
    # print(filtered_signal[channel, -10:])

    # Plot the spectra, limited to positive frequencies
    mask = freq >= 0
    plt.plot(freq[mask], np.abs(X[mask]))

    plt.xlabel("Frequency (Hz)")
    plt.ylabel("Magnitude")
    plt.title(f"Channel {channel}: Spectrum of Input and Output Signals")
    plt.legend(["Input Signal", "Filtered Output Signal"])
    plt.show()


def plot_audio_spectrum_from_file(
    input_signal_file,
    filtered_signal_file,
    num_channels,
    num_samples,
    channel=0,
    fs=48e3,
):

    with open(input_signal_file, "r") as f:
        input_signal = np.array([float(line.strip()) for line in f.readlines()])

    with open(filtered_signal_file, "r") as f:
        filtered_signal = np.array([float(line.strip()) for line in f.readlines()])

    input_signal = input_signal.reshape((num_samples, num_channels))
    filtered_signal = filtered_signal.reshape((num_samples, num_channels))

    print(input_signal.shape)
    print(filtered_signal.shape)

    plot_audio_spectrum(
        np.transpose(input_signal), np.transpose(filtered_signal), channel
    )


def plot_audio_spectrum_from_wav_file(
    input_signal_file,
    filtered_signal_file,
    num_channels,
    num_samples,
    channel=0,
    fs=48e3,
):

    input_signal, fs = sf.read(input_signal_file, dtype="int16")
    filtered_signal, fs = sf.read(filtered_signal_file, dtype="int16")

    print(input_signal.shape)

    plot_audio_spectrum(
        np.transpose((input_signal)), np.transpose(filtered_signal), channel
    )


def plot_audio_spectrum_from_txt_wav_file(
    input_signal_file,
    filtered_signal_file,
    num_channels,
    num_samples,
    channel=0,
    fs=48e3,
):

    with open(input_signal_file, "r") as f:
        input_signal = np.array([float(line.strip()) for line in f.readlines()])

    input_signal = input_signal.reshape((num_samples, num_channels))
    filtered_signal, fs = sf.read(filtered_signal_file, dtype="int16")

    plot_audio_spectrum(
        np.transpose((input_signal)), np.transpose(filtered_signal), channel
    )


def plot_audio_spectrum_from_wav_txt_file(
    input_signal_file,
    filtered_signal_file,
    num_channels,
    num_samples,
    channel=0,
    fs=48e3,
):

    input_signal, fs = sf.read(input_signal_file, dtype="int16")
    with open(filtered_signal_file, "r") as f:
        filtered_signal = np.array([float(line.strip()) for line in f.readlines()])

    filtered_signal = filtered_signal.reshape((num_samples, num_channels))

    plot_audio_spectrum(
        np.transpose((input_signal)), np.transpose(filtered_signal), channel
    )
