import numpy as np
import sys
import os
import argparse
import matplotlib.pyplot as plt
from utils import debug_print, generate_spectral_test_signal, plot_signals

# TODO: Add the necessary paths to audiolib and dsplib to fetch idat_gen python packages
# Get the absolute path to the project root
project_root = os.path.join(os.environ.get("HOME"), "tisp")
sys.path.append(os.path.join(project_root, "test"))  # path to tisp test directory

sys.path.append(
    os.path.join(project_root, "external/audiolib/test/")
)  # path to audiolib test directory
sys.path.append(
    os.path.join(project_root, "external/dsplib/test/")
)  # path to dsplib test directory

sys.path.append(
    os.path.join(project_root, "test/TISP_idat_gen/audio_signal_chain/parametric_eq/")
)  # path to parametric equalizer module

import AUDIOLIB_idat_gen as audiolib
import DSPLIB_idat_gen as dsplib
import TISP_idat_gen as tisp
import parametric_eq as peq
from common.TISP_file_io import TensorToCppHeader


def main(args):
    # Get verbose flag from command line arguments
    verbose = args.v

    # Signal chain parameters
    fs = 48000
    effective_bits = 20  # Effective bit depth to limit the signal to

    # Define buffer parameters
    NUM_CHANNELS_IN = int(8)
    NUM_CHANNELS_OUT = int(12)
    NUM_SAMPLES = int(4096)
    BLOCK_SIZE = int(64)
    NUM_FRAMES = int(np.ceil(NUM_SAMPLES / BLOCK_SIZE))

    # Define filter parameters for both cascadeBiquad and parametric EQ
    # Low-shelf: boost bass frequencies below 200 Hz by 1 dB
    low_shelf_params = (200, 1, 0.7071)
    # Notch: cut frequencies around 1 kHz with a narrow Q
    notch_params = (1000, 5.0)
    # High-shelf: cut high frequencies above 5 kHz by 3 dB
    high_shelf_params = (5000, -3, 0.7071)

    ###################################################
    # Initialize all modules for the signal chain
    ###################################################

    # 1. typeconversion (int32_t to float)
    k0 = audiolib.AUDIOLIB_typeConversion(
        inDtype="int32_t",
        outDtype="float",
        inChannels=NUM_SAMPLES,
        inSamples=NUM_CHANNELS_IN,
    )

    # 2. gainNCh
    # Make targetGain programmable with NUM_CHANNELS_IN
    targetGain = np.array([0.5] * NUM_CHANNELS_IN)
    k1 = audiolib.AUDIOLIB_gainNCh(
        isInterleave=1, inChannels=NUM_CHANNELS_IN, inSamples=NUM_SAMPLES
    )

    # 3. cascadebiquad (using dsplib.DSPLIB_cascadeBiquad)
    k2 = dsplib.DSPLIB_cascadeBiquad(
        dataType="float",
        numStages=3,  # 3 stages for low-shelf, notch, and high-shelf
        numChannels=NUM_CHANNELS_IN,
        numSamples=NUM_SAMPLES,
    )

    # 4. 8ch to 8ch router
    # Define router configuration
    # For any additional channels beyond NUM_CHANNELS_IN, we'll duplicate input channels in a cyclic manner
    routerConfig = [(i % NUM_CHANNELS_IN) for i in range(NUM_CHANNELS_OUT)]
    k3 = audiolib.AUDIOLIB_router(
        dType="float",
        inChannels=NUM_CHANNELS_IN,
        inSamples=NUM_SAMPLES,
        isInterleave=1,
        routerConfig=routerConfig,
    )

    # 5. parametric eq (using peq.ParametricEQ)
    k4 = peq.ParametricEQ(fs=fs, num_channels=NUM_CHANNELS_OUT)
    # Design the parametric EQ
    sos_coeffs = k4.design_parametric_eq(
        low_shelf_params, notch_params, high_shelf_params
    )

    # 6. delayNCh
    delaySizes = np.zeros(NUM_CHANNELS_OUT, dtype=np.int32)
    k5 = audiolib.AUDIOLIB_delayNChannel(
        dType="float",
        mode=0,
        interleave=1,
        numChannels=NUM_CHANNELS_OUT,
        numSamples=BLOCK_SIZE,
        numExecReps=NUM_FRAMES,
        delaySize=delaySizes,
    )

    # 7. balanceFader
    # Define balanceFader parameters
    balance = 0.2  # Slightly right balance
    fader = 0.1  # Slightly front fader
    sideGainFactor = 0.7  # Gain factor for side channels
    lfeBalanceGainFactor = 0.8  # Gain factor for LFE balance
    lfeFaderGainFactor = 0.6  # Gain factor for LFE fader
    lfeBalanceMode = 1  # LFE balance mode (1=partial)
    lfeFaderMode = 2  # LFE fader mode (2=position_based)

    k6 = audiolib.AUDIOLIB_balanceFader(
        dType="float",
        numSamples=NUM_SAMPLES,
        numChannels=NUM_CHANNELS_OUT,
        balance=balance,
        fader=fader,
        sideGainFactor=sideGainFactor,
        lfeBalanceGainFactor=lfeBalanceGainFactor,
        lfeFaderGainFactor=lfeFaderGainFactor,
        lfeBalanceMode=lfeBalanceMode,
        lfeFaderMode=lfeFaderMode,
        isInterleave=False,  # Set to False to handle the data as non-interleaved
    )

    # 8. gainNchTrim
    channelGains = np.full(NUM_CHANNELS_OUT, 0.9)
    masterGain = 0.95
    k7 = audiolib.AUDIOLIB_gainNChTrim(
        dType="float",
        inChannels=NUM_CHANNELS_OUT,
        inSamples=NUM_SAMPLES,
        isInterleave=1,
    )

    # 9. limiter
    k8 = tisp.AUDIOLIB.TISP_limiter(
        libRootDir=project_root, blockSize=NUM_SAMPLES, numChannels=NUM_CHANNELS_OUT
    )
    # Define limiter parameter buffers
    threshold_dB = np.full(NUM_CHANNELS_OUT, -3.0)  # -3 dB threshold
    kneeWidth_dB = np.full(NUM_CHANNELS_OUT, 6.0)  # 6 dB knee width
    attackCoeff = np.full(NUM_CHANNELS_OUT, 0.9)  # Fast attack
    releaseCoeff = np.full(NUM_CHANNELS_OUT, 0.9)  # Slow release

    # 10. muteNCh
    # Make targetGainMute and initialGainMute programmable with NUM_CHANNELS_OUT
    targetGainMute = np.ones(NUM_CHANNELS_OUT)
    initialGainMute = np.ones(NUM_CHANNELS_OUT)
    k9 = audiolib.AUDIOLIB_muteNCh(
        numChannels=NUM_CHANNELS_OUT,
        numSamples=BLOCK_SIZE,
        numExecReps=NUM_FRAMES,
        fadeType="HARD",
        isInterleaved=True,
        samplingRate=fs,
        targetGain=targetGainMute,
        initialGain=initialGainMute,
    )

    # 11. typeconversion (float to int32_t)
    k10 = audiolib.AUDIOLIB_typeConversion(
        inDtype="float",
        outDtype="int32_t",
        inChannels=NUM_SAMPLES,
        inSamples=NUM_CHANNELS_OUT,
    )

    # Create test signal based on the selected signal type
    if args.signal_type:
        input_signal = generate_spectral_test_signal(
            fs, NUM_SAMPLES, NUM_CHANNELS_IN, signal_type=args.signal_type
        )
        debug_print(
            f"Generated {args.signal_type} test signal for spectral analysis", verbose
        )
    else:

        t, x_q24 = peq.generate_test_signal(
            fs,
            duration=NUM_SAMPLES / fs,
            num_channels=NUM_CHANNELS_IN,
            effective_bits=effective_bits,
        )
        input_signal = x_q24

    ###################################################
    # Execute signal chain
    ###################################################

    # 1. typeconversion (int32_t to float)
    signal_1 = k0.exec(input_signal)
    debug_print(f"After typeConversion (int32->float) shape: {signal_1.shape}", verbose)
    debug_print(
        f"Signal 1 min: {np.min(signal_1)}, max: {np.max(signal_1)}, mean: {np.mean(signal_1)}",
        verbose,
    )

    # 2. gainNCh
    signal_2 = k1.exec(signal_1, targetGain)
    debug_print(f"After gainNCh shape: {signal_2.shape}", verbose)
    debug_print(
        f"Signal 2 min: {np.min(signal_2)}, max: {np.max(signal_2)}, mean: {np.mean(signal_2)}",
        verbose,
    )

    # 3. cascadebiquad
    # Create parametric EQ SOS coefficients for cascadeBiquad
    param_eq_temp = peq.ParametricEQ(fs=fs, num_channels=NUM_CHANNELS_IN)
    sos_coeffs_cb = param_eq_temp.design_parametric_eq(
        low_shelf_params, notch_params, high_shelf_params
    )

    # Reshape the SOS coefficients to match the expected shape for cascadeBiquad
    # The cascadeBiquad module expects a 3D array with shape (numStages, NUM_COEFS_PER_BIQUAD_FULL, filterNumChannels)
    # But the ParametricEQ.design_parametric_eq returns a 2D array with shape (numStages, NUM_COEFS_PER_BIQUAD_FULL)
    # So we need to reshape it to add the channel dimension
    sos_coeffs_cb_3d = np.zeros((sos_coeffs_cb.shape[0], sos_coeffs_cb.shape[1], 1))
    sos_coeffs_cb_3d[:, :, 0] = sos_coeffs_cb

    # Set the coefficients for cascadeBiquad
    k2.set_filter_coefficients(sos_coeffs_cb_3d)
    signal_3 = k2.exec(signal_2)
    debug_print(f"After cascadeBiquad shape: {signal_3.shape}", verbose)
    debug_print(
        f"Signal 3 min: {np.min(signal_3)}, max: {np.max(signal_3)}, mean: {np.mean(signal_3)}",
        verbose,
    )

    # 4. 8ch to 12ch router
    signal_4 = k3.exec(signal_3)
    debug_print(
        f"After {NUM_CHANNELS_IN}ch to {NUM_CHANNELS_OUT}ch router shape: {signal_4.shape}",
        verbose,
    )
    debug_print(
        f"Signal 4 min: {np.min(signal_4)}, max: {np.max(signal_4)}, mean: {np.mean(signal_4)}",
        verbose,
    )

    # 5. parametric eq
    signal_5 = k4.apply_filter(signal_4)
    debug_print(f"After parametric eq shape: {signal_5.shape}", verbose)
    debug_print(
        f"Signal 5 min: {np.min(signal_5)}, max: {np.max(signal_5)}, mean: {np.mean(signal_5)}",
        verbose,
    )

    # 6. delayNCh
    signal_6, delay, delaySize = k5.exec(signal_5)
    debug_print(f"After delayNCh shape: {signal_6.shape}", verbose)
    debug_print(
        f"Signal 6 min: {np.min(signal_6)}, max: {np.max(signal_6)}, mean: {np.mean(signal_6)}",
        verbose,
    )

    # 7. balanceFader
    result = k6.exec(signal_6.T)
    signal_7 = result[0]  # Extract the output signal from the tuple
    signal_7_channelConfig = result[1]  # Extract the channel config from the tuple
    signal_7 = signal_7.reshape(NUM_CHANNELS_OUT, -1).T

    debug_print(f"After balanceFader shape: {signal_7.shape}", verbose)
    debug_print(
        f"Signal 7 min: {np.min(signal_7)}, max: {np.max(signal_7)}, mean: {np.mean(signal_7)}",
        verbose,
    )

    # 8. gainNchTrim
    signal_8 = k7.exec(signal_7, channelGains, masterGain)
    debug_print(f"After gainNchTrim shape: {signal_8.shape}", verbose)
    debug_print(
        f"Signal 8 min: {np.min(signal_8)}, max: {np.max(signal_8)}, mean: {np.mean(signal_8)}",
        verbose,
    )

    # 9. limiter
    signal_9, gain = k8.exec(
        signal_8, threshold_dB, kneeWidth_dB, attackCoeff, releaseCoeff
    )
    debug_print(f"After limiter shape: {signal_9.shape}", verbose)
    debug_print(
        f"Signal 9 min: {np.min(signal_9)}, max: {np.max(signal_9)}, mean: {np.mean(signal_9)}",
        verbose,
    )

    # 10. muteNCh
    # Reshape signal for muteNCh module (expects shape [numExecReps, numSamples * numChannels])
    signal_9_reshaped = signal_9.reshape(int(NUM_SAMPLES / BLOCK_SIZE), -1)
    debug_print(f"After reshape for muteNCh shape: {signal_9_reshaped.shape}", verbose)
    debug_print(
        f"Signal 9 reshaped min: {np.min(signal_9_reshaped)}, max: {np.max(signal_9_reshaped)}, mean: {np.mean(signal_9_reshaped)}",
        verbose,
    )

    signal_10 = k9.exec(signal_9_reshaped)
    # Reshape back to original format
    signal_10 = signal_10.reshape(signal_9.shape)
    debug_print(f"After muteNCh shape: {signal_10.shape}", verbose)
    debug_print(
        f"Signal 10 min: {np.min(signal_10)}, max: {np.max(signal_10)}, mean: {np.mean(signal_10)}",
        verbose,
    )

    # 11. typeconversion (float to int32_t)
    output_signal = k10.exec(signal_10)
    debug_print(
        f"Final output (after typeConversion float->int32) shape: {output_signal.shape}",
        verbose,
    )
    debug_print(
        f"Output signal min: {np.min(output_signal)}, max: {np.max(output_signal)}, mean: {np.mean(output_signal)}",
        verbose,
    )

    # Count non-zero values in the output signal
    non_zero_count = np.count_nonzero(output_signal)
    debug_print(
        f"Number of non-zero values in output signal: {non_zero_count} out of {output_signal.size}",
        verbose,
    )

    ###################################################
    # Write data to header file
    ###################################################

    test_case = str(1)
    header_file_name = (
        "../../../audio_signal_chain/TISP_11_02_signal_chain/test_data/staticCase"
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
    k4.write_coeffs_to_file(header_file_name, "cascadeBiquad0CoeffCase" + test_case)

    # write gain buffers
    tensor_to_cpp_header.write_tensor_to_header(
        targetGain, "targetGain" + test_case, "float"
    )

    tensor_to_cpp_header.write_tensor_to_header(
        channelGains, "channelGains" + test_case, "float"
    )

    # Write limiter parameters
    tensor_to_cpp_header.write_tensor_to_header(
        threshold_dB, "threshold_dB" + test_case, "float"
    )

    tensor_to_cpp_header.write_tensor_to_header(
        kneeWidth_dB, "kneeWidth_dB" + test_case, "float"
    )

    tensor_to_cpp_header.write_tensor_to_header(
        attackCoeff, "attackCoeff" + test_case, "float"
    )

    tensor_to_cpp_header.write_tensor_to_header(
        releaseCoeff, "releaseCoeff" + test_case, "float"
    )

    # Write delaySizes as int32_t
    tensor_to_cpp_header.write_tensor_to_header(
        delaySizes, "delaySizes" + test_case, "int32_t"
    )

    # Write balanceFader float parameters
    balanceFaderFloatParams = np.array(
        [balance, fader, lfeBalanceGainFactor, lfeFaderGainFactor, sideGainFactor],
        dtype=np.float32,
    )
    tensor_to_cpp_header.write_tensor_to_header(
        balanceFaderFloatParams, "balanceFaderFloatParams" + test_case, "float"
    )

    # Write balanceFader int parameters
    balanceFaderIntParams = np.array([lfeBalanceMode, lfeFaderMode], dtype=np.int32)
    tensor_to_cpp_header.write_tensor_to_header(
        balanceFaderIntParams, "balanceFaderIntParams" + test_case, "int32_t"
    )

    # Write balanceFader channel layout
    # Get the channel config integers from the AUDIOLIB_CHANNEL_MAP
    channelConfigInts = np.array(signal_7_channelConfig).astype(np.int32)
    tensor_to_cpp_header.write_tensor_to_header(
        channelConfigInts, "balanceFaderChannelConfig" + test_case, "int32_t"
    )

    # Write router configuration
    routerConfigArray = np.array(routerConfig, dtype=np.int32)
    tensor_to_cpp_header.write_tensor_to_header(
        routerConfigArray, "routerConfig" + test_case, "int32_t"
    )

    ###################################################
    # Print a summary of the parameter buffers
    ###################################################

    debug_print("\nParameter Buffers Summary:", verbose)
    debug_print(
        f"targetGain (for gainNCh): {targetGain.shape} - Values: {targetGain}", verbose
    )
    debug_print(
        f"channelGains (for gainNchTrim): {channelGains.shape} - Values: {channelGains}",
        verbose,
    )
    debug_print(
        f"threshold_dB (for limiter): {threshold_dB.shape} - Values: {threshold_dB}",
        verbose,
    )
    debug_print(
        f"kneeWidth_dB (for limiter): {kneeWidth_dB.shape} - Values: {kneeWidth_dB}",
        verbose,
    )
    debug_print(
        f"attackCoeff (for limiter): {attackCoeff.shape} - Values: {attackCoeff}",
        verbose,
    )
    debug_print(
        f"releaseCoeff (for limiter): {releaseCoeff.shape} - Values: {releaseCoeff}",
        verbose,
    )
    debug_print(
        f"delaySizes (for delayNCh): {delaySizes.shape} - Values: {delaySizes}", verbose
    )
    debug_print(
        f"routerConfig (for router): {len(routerConfig)} - Values: {routerConfig}",
        verbose,
    )

    # Plot input and output signals if plotting is enabled
    if args.plot:
        plot_signals(
            input_signal,
            output_signal,
            fs,
            NUM_CHANNELS_IN,
            NUM_CHANNELS_OUT,
            args.save_plots,
        )


# run application
if __name__ == "__main__":
    # Parse command line arguments
    parser = argparse.ArgumentParser(
        description="Run the 11_02 signal chain with optional verbose output."
    )
    parser.add_argument(
        "-v", action="store_true", help="Enable verbose output (debug mode)"
    )
    parser.add_argument(
        "-p", "--plot", action="store_true", help="Plot input and output signals"
    )
    parser.add_argument("--save-plots", action="store_true", help="Save plots to files")
    parser.add_argument(
        "--signal-type",
        choices=["sweep", "white", "pink", "impulse", "multitone"],
        help="Type of test signal to use for spectral analysis",
    )
    args = parser.parse_args()

    # Run the main function with parsed arguments
    main(args)
