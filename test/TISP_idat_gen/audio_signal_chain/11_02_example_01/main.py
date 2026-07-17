import numpy as np
import sys
import os

# Get the absolute path to the project root
project_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../../../"))

# Add the necessary paths
sys.path.append(os.path.join(project_root, "external/audiolib/test/"))
sys.path.append(os.path.join(project_root, "external/dsplib/test/"))
sys.path.append(os.path.join(os.path.dirname(__file__), "../parametric_eq/"))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../../"))

import AUDIOLIB_idat_gen as audiolib
import DSPLIB_idat_gen as dsplib
import parametric_eq as tisp
from common.TISP_file_io import TensorToCppHeader


def main():

    # Cascadebiquad
    fs = 48000
    effective_bits = 20  # Effective bit depth to limit the signal to

    NUM_CHANNELS_IN = int(8)
    NUM_CHANNELS_OUT = int(12)
    NUM_SAMPLES = int(4096)

    BLOCK_WIDTH = NUM_CHANNELS_IN
    BLOCK_HEIGHT = NUM_SAMPLES

    # Initialize all modules first
    k0 = audiolib.AUDIOLIB_typeConversion(
        inDtype="int32_t",
        outDtype="float",
        inChannels=BLOCK_HEIGHT,
        inSamples=BLOCK_WIDTH,
    )

    # Add cascadeBiquad module after typeConversion
    k1 = tisp.ParametricEQ(fs=fs, num_channels=NUM_CHANNELS_IN)

    # Define filter parameters
    # Low-shelf: boost bass frequencies below 200 Hz by 1 dB
    low_shelf_params = (200, 1, 0.7071)

    # Notch: cut frequencies around 1 kHz with a narrow Q
    notch_params = (1000, 5.0)

    # High-shelf: cut high frequencies above 5 kHz by 3 dB
    high_shelf_params = (5000, -3, 0.7071)

    # Design the parametric EQ
    sos_coeffs = k1.design_parametric_eq(
        low_shelf_params, notch_params, high_shelf_params
    )

    # Define gain values for gainNCh module
    targetGain = np.array([0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5])

    k2 = audiolib.AUDIOLIB_gainNCh(
        isInterleave=1, inChannels=BLOCK_WIDTH, inSamples=BLOCK_HEIGHT
    )

    targetGainMute = np.ones(NUM_CHANNELS_IN)
    initialGainMute = np.ones(NUM_CHANNELS_IN)

    k3 = audiolib.AUDIOLIB_muteNCh(
        numChannels=BLOCK_WIDTH,
        numSamples=BLOCK_HEIGHT,
        numExecReps=1,  # Only one execution repetition
        fadeType="HARD",
        isInterleaved=True,
        initialGain=initialGainMute,
        targetGain=targetGainMute,
    )

    k4 = audiolib.AUDIOLIB_typeConversion(
        inDtype="float",
        outDtype="int32_t",
        inChannels=BLOCK_HEIGHT,
        inSamples=BLOCK_WIDTH,
    )

    # Create interleaved input data (MCASP-simulated)

    # Generate a simulated Q24 ADC test signal
    t, x_q24 = tisp.generate_test_signal(
        fs,
        duration=NUM_SAMPLES / fs,
        num_channels=NUM_CHANNELS_IN,
        effective_bits=effective_bits,
    )

    input_signal = x_q24

    # Execute signal chain
    k0_signal = k0.exec(input_signal)
    print(f"After k0 (typeConversion int32->float) shape: {k0_signal.shape}")

    k1_signal = k1.apply_filter(k0_signal)
    print(f"After k1 (cascadeBiquad) shape: {k1_signal.shape}")

    k2_signal = k2.exec(k1_signal, targetGain)
    print(f"After k2 (gainNCh) shape: {k2_signal.shape}")

    # Reshape k2_signal for muteNCh module (expects shape [numExecReps, numSamples * numChannels])
    k2_signal_reshaped = k2_signal.reshape(1, -1)
    print(f"After reshape for muteNCh shape: {k2_signal_reshaped.shape}")

    k3_signal = k3.exec(k2_signal_reshaped)
    print(f"After k3 (muteNCh) shape: {k3_signal.shape}")

    # Reshape back to original format
    k3_signal = k3_signal.reshape(k2_signal.shape)
    print(f"After reshape back to original shape: {k3_signal.shape}")

    output_signal = k4.exec(k3_signal)

    print(
        f"Final output (after k4 typeConversion float->int32) shape: {output_signal.shape}"
    )

    # Write data to header file
    test_case = str(1)
    header_file_name = (
        "../../../audio_signal_chain/TISP_11_02_example_01/test_data/staticCase"
        + test_case
        + ".hpp"
    )

    tensor_to_cpp_header = TensorToCppHeader(header_file_name, mode="new")

    # write input signal
    tensor_to_cpp_header.write_tensor_to_header(
        input_signal, "inputSignalCase" + test_case, "int32_t"
    )

    # write output signal
    tensor_to_cpp_header.write_tensor_to_header(
        output_signal, "outputSignalCase" + test_case, "int32_t"
    )

    # write biquad coeffs in TI form
    k1.write_coeffs_to_file(header_file_name, "cascadeBiquad0CoeffCase" + test_case)

    # write gain buffers
    tensor_to_cpp_header.write_tensor_to_header(
        targetGain, "targetGain" + test_case, "float"
    )

    tensor_to_cpp_header.write_tensor_to_header(
        initialGainMute, "initialGainMute" + test_case, "float"
    )

    tensor_to_cpp_header.write_tensor_to_header(
        targetGainMute, "targetGainMute" + test_case, "float"
    )


# run example
if __name__ == "__main__":
    main()
