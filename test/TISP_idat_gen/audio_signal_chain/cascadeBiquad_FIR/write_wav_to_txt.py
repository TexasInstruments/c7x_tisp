import soundfile as sf
import sys


def write_wav_to_txt(wav_file, txt_file, channel, d_type="int16"):

    data, fs = sf.read(wav_file, dtype="int16")

    with open(txt_file, "w") as f:
        for value in data[:, channel]:
            f.write(str(value) + "\n")


if __name__ == "__main__":

    wav_file = str(sys.argv[1])
    txt_file = str(sys.argv[2])
    channel = int(sys.argv[3])

    write_wav_to_txt(wav_file, txt_file, channel)
