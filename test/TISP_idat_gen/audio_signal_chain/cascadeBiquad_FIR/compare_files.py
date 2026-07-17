import numpy as np
import sys


def compare_files(data_file, gold_file, tolerance, dtype="int16"):
    data = np.loadtxt(data_file).astype(dtype)
    golden_val = np.loadtxt(gold_file).astype(dtype)

    differences = np.abs(data - golden_val)
    not_close = np.where(differences > tolerance)[0]

    if len(not_close) == 0:
        print(f"All values are within tolerance of {tolerance}")
    else:
        print("Values not within tolerance:")
        for i in not_close:
            print(
                f"Index {i}: {data[i]} vs {golden_val[i]}, difference: {differences[i]}"
            )

    # Calculate SNR

    # signal_power = np.mean(np.power(golden_val.astype("float64"), 2))
    # noise_power = np.mean(
    #     np.abs(data.astype("float64") - golden_val.astype("float64")) ** 2
    # )

    signal_power = np.max(np.power(golden_val.astype("float64"), 2))
    noise_power = np.max(
        np.abs(data.astype("float64") - golden_val.astype("float64")) ** 2
    )

    print(signal_power)
    print(noise_power)

    snr = 10 * np.log10(signal_power / noise_power)
    print(f"SNR: {snr} dB")
    print(max(golden_val))
    print(max(data))

    print(min(golden_val))
    print(min(data))


if __name__ == "__main__":
    tolerance = int(sys.argv[3])
    compare_files(sys.argv[1], sys.argv[2], tolerance)
