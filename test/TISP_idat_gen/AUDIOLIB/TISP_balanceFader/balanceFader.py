# Copyright (C) 2026 Texas Instruments Incorporated
#
# SPDX-License-Identifier: LicenseRef-TI-TFL

import json

import numpy as np

import AUDIOLIB_channelMappings


class BalanceFader:
    """Reference model of the balanceFader kernel (balance/fader channel gains)."""

    # Dictionary to map test types to (f_start, f_end) tuples
    FREQUENCY_RANGES = {
        "SWEEP": (1000.0, 10000.0),  # SWEEP: from 2000 Hz to 20000 Hz
        "SINE": None,  # SINE: use frequency from CSV
        "STATIC": None,  # STATIC: use frequency from CSV
        # "RANDOM": None,            # Optional: for completeness, if RANDOM test cases exist
    }

    def __init__(
        self,
        *,
        num_samples,
        num_channels,
        in_dtype,
        is_interleaved,
        balance,
        fade,
        sideGainFactor,
        lfeBalanceGainFactor,
        lfeFaderGainFactor,
        lfeBalanceMode,
        lfeFaderMode,
        pChannelConfig,
        bypassMode=0,
        **_,
    ):
        self.num_samples = int(num_samples)
        self.num_channels = int(num_channels)
        self.dtype = np.dtype(in_dtype)
        self.is_interleaved = bool(is_interleaved)
        self.balance = balance
        self.fade = fade
        self.sideGainFactor = sideGainFactor
        self.lfeBalanceGainFactor = lfeBalanceGainFactor
        self.lfeFaderGainFactor = lfeFaderGainFactor
        self.lfeBalanceMode = lfeBalanceMode
        self.lfeFaderMode = lfeFaderMode
        # pChannelConfig: the AUDIOLIB_CHANNEL_* enum keys, one per channel
        self.channel_config_integers = [int(k) for k in np.asarray(pChannelConfig).reshape(-1)]
        self.bypassMode = bypassMode
        self.init()

    @staticmethod
    def get_channel_layout(numChannels):
        """Define the speaker configuration for a given number of channels."""
        base_layout = [
            "FRONT_LEFT_TREBBLE",
            "FRONT_RIGHT_TREBBLE",
            "FRONT_CENTER",
            "LOW_FREQUENCY_EFFECTS",
            "SIDE_LEFT",
            "SIDE_RIGHT",
            "REAR_LEFT",
            "REAR_RIGHT",
            "FRONT_LEFT_MID",
            "FRONT_RIGHT_MID",
            "TOP_FRONT_LEFT",
            "TOP_FRONT_RIGHT",
            "FRONT_LEFT_WOOFER",
            "FRONT_RIGHT_WOOFER",
            "TOP_REAR_LEFT",
            "TOP_REAR_RIGHT",
        ]
        full_layout = (base_layout * (numChannels // len(base_layout) + 1))[:numChannels]
        return full_layout


    @staticmethod
    def load_channel_layouts(json_file_path="channel_layouts.json"):
        """Load channel layouts from a JSON file."""
        try:
            with open(json_file_path, "r") as f:
                data = json.load(f)
            return data.get("layouts", {})
        except (FileNotFoundError, json.JSONDecodeError) as e:
            print(f"Error loading channel_layouts.json: {e}. Using default layout.")
            return {}


    @staticmethod
    def generate_sine(
        num_samples, sampling_rate, num_frames, channel, frequency, amplitude, dtype
    ):
        """Generate a continuous sine wave for all samples of a given channel.

        Args:
            num_samples (int): Number of samples per frame.
            sampling_rate (int): Sampling rate in Hz.
            num_frames (int): Number of numExecReps.
            channel (int): Channel index (for phase offset if needed).
            frequency (float, optional): Frequency of the sine wave in Hz. Defaults to 2000.0 Hz.
            amplitude (float, optional): Amplitude of the sine wave. Defaults to 1.0.

        Returns:
            numpy.ndarray: Array of sine wave samples, shape (num_samples * num_frames,).
        """
        total_samples = num_samples * num_frames
        t = np.linspace(0, total_samples / sampling_rate, total_samples, endpoint=False)
        sine_wave = amplitude * np.sin(2 * np.pi * frequency * t)
        return sine_wave.astype(dtype)


    @staticmethod
    def generate_sweep(
        num_samples, sampling_rate, num_frames, channel, amplitude, f_start, f_end, dtype
    ):
        """Generate a linear frequency sweep for all num_frames of a given channel.

        Args:
            num_samples (int): Number of samples per frame.
            sampling_rate (int): Sampling rate in Hz.
            num_frames (int): Number of frames.
            channel (int): Channel index (for phase offset).
            amplitude (float): Amplitude of the sweep signal.
            f_start (float): Starting frequency in Hz.
            f_end (float, optional): Ending frequency in Hz. Defaults to f_start * 10.

        Returns:
            numpy.ndarray: Array of sweep signal samples, shape (num_samples * num_frames,).
        """
        total_samples = num_samples * num_frames
        duration = total_samples / sampling_rate  # Total duration in seconds
        t = np.linspace(0, duration, total_samples, endpoint=False)

        # Linear sweep: f(t) = f_start + (f_end - f_start)/duration * t
        k = (f_end - f_start) / duration
        phase = 2 * np.pi * (f_start * t + 0.5 * k * t**2)
        phase_offset = channel * np.pi / 4  # 45-degree offset per channel
        sweep_signal = amplitude * np.sin(phase + phase_offset)

        return sweep_signal.astype(dtype)


    def init(self):
        """Compute the per-channel gains from the balance/fade settings (called by the constructor)."""
        balance = self.balance
        fader = self.fade
        lfeBalanceMode = self.lfeBalanceMode
        lfeFaderMode = self.lfeFaderMode
        numChannels = self.num_channels
        sideGainFactor = self.sideGainFactor
        lfeBalanceGainFactor = self.lfeBalanceGainFactor
        lfeFaderGainFactor = self.lfeFaderGainFactor

        CMAP = AUDIOLIB_channelMappings.AUDIOLIB_CHANNEL_MAP
        LFE_BALANCE_MAP = AUDIOLIB_channelMappings.AUDIOLIB_LFE_BALANCE_MODE_MAP
        LFE_FADER_MAP = AUDIOLIB_channelMappings.AUDIOLIB_LFE_FADER_MODE_MAP
        dtype = self.dtype

        # --- 1. Map the channel enum keys to speaker names ---
        enum_to_name = {v: k[len("AUDIOLIB_CHANNEL_"):] for k, v in CMAP.items()}
        channel_config_integers = self.channel_config_integers
        speaker_layout_names = [enum_to_name[e] for e in channel_config_integers]
        self.speaker_layout_names = speaker_layout_names

        # --- 2. Calculate Fundamental Gain Components ---
        balance_angle = (np.pi / 4.0) * (balance + 1)
        targetGainL = np.cos(balance_angle)
        targetGainR = np.sin(balance_angle)

        fader_angle = (np.pi / 4.0) * (fader + 1)
        frontGain = np.sin(fader_angle)
        backGain = np.cos(fader_angle)
        sideGain = (
            sideGainFactor * backGain + (1 - sideGainFactor) * frontGain
            if fader < 0
            else backGain
        )
        topMiddleGain = 0.5 * (frontGain + backGain)

        # --- 3. Calculate Final Gain for Each Channel in the Layout ---
        final_channel_gains = np.zeros(numChannels, dtype=dtype)
        for i, channel_name in enumerate(speaker_layout_names):
            channel_enum = CMAP[f"AUDIOLIB_CHANNEL_{channel_name}"]
            gain = 0.0

            front_left_channels = [
                CMAP["AUDIOLIB_CHANNEL_FRONT_LEFT"],
                CMAP["AUDIOLIB_CHANNEL_FRONT_LEFT_TREBBLE"],
                CMAP["AUDIOLIB_CHANNEL_FRONT_LEFT_MID"],
                CMAP["AUDIOLIB_CHANNEL_FRONT_LEFT_WOOFER"],
            ]
            front_right_channels = [
                CMAP["AUDIOLIB_CHANNEL_FRONT_RIGHT"],
                CMAP["AUDIOLIB_CHANNEL_FRONT_RIGHT_TREBBLE"],
                CMAP["AUDIOLIB_CHANNEL_FRONT_RIGHT_MID"],
                CMAP["AUDIOLIB_CHANNEL_FRONT_RIGHT_WOOFER"],
            ]

            if channel_enum in front_left_channels:
                gain = targetGainL * frontGain
            elif channel_enum in front_right_channels:
                gain = targetGainR * frontGain
            elif channel_enum == CMAP["AUDIOLIB_CHANNEL_FRONT_CENTER"]:
                gain = 1.0 * frontGain
            elif channel_enum == CMAP["AUDIOLIB_CHANNEL_LOW_FREQUENCY_EFFECTS"]:
                lfeBalanceGain = 1.0
                if lfeBalanceMode == LFE_BALANCE_MAP["AUDIOLIB_LFE_BALANCE_PARTIAL"]:
                    lfeBalanceGain = lfeBalanceGainFactor + (
                        1 - lfeBalanceGainFactor
                    ) * np.cos(2 * balance_angle)
                elif lfeBalanceMode == LFE_BALANCE_MAP["AUDIOLIB_LFE_BALANCE_FULL"]:
                    lfeBalanceGain = 0.5 * (targetGainL + targetGainR)

                lfeFaderGain = 1.0
                if lfeFaderMode == LFE_FADER_MAP["AUDIOLIB_LFE_FADER_AVERAGE"]:
                    lfeFaderGain = 0.5 * (frontGain + backGain)
                elif lfeFaderMode == LFE_FADER_MAP["AUDIOLIB_LFE_FADER_POSITION_BASED"]:
                    lfeFaderGain = (
                        1 - lfeFaderGainFactor
                    ) * frontGain + lfeFaderGainFactor * backGain
                elif lfeFaderMode == LFE_FADER_MAP["AUDIOLIB_LFE_FADER_FULL"]:
                    lfeFaderGain = backGain
                gain = lfeBalanceGain * lfeFaderGain
            elif channel_enum == CMAP["AUDIOLIB_CHANNEL_SIDE_LEFT"]:
                gain = targetGainL * sideGain
            elif channel_enum == CMAP["AUDIOLIB_CHANNEL_SIDE_RIGHT"]:
                gain = targetGainR * sideGain
            elif channel_enum == CMAP["AUDIOLIB_CHANNEL_REAR_LEFT"]:
                gain = targetGainL * backGain
            elif channel_enum == CMAP["AUDIOLIB_CHANNEL_REAR_RIGHT"]:
                gain = targetGainR * backGain
            elif channel_enum == CMAP["AUDIOLIB_CHANNEL_TOP_FRONT_LEFT"]:
                gain = targetGainL * frontGain
            elif channel_enum == CMAP["AUDIOLIB_CHANNEL_TOP_FRONT_RIGHT"]:
                gain = targetGainR * frontGain
            elif channel_enum == CMAP["AUDIOLIB_CHANNEL_TOP_MIDDLE_LEFT"]:
                gain = targetGainL * topMiddleGain
            elif channel_enum == CMAP["AUDIOLIB_CHANNEL_TOP_MIDDLE_RIGHT"]:
                gain = targetGainR * topMiddleGain
            elif channel_enum == CMAP["AUDIOLIB_CHANNEL_TOP_REAR_LEFT"]:
                gain = targetGainL * backGain
            elif channel_enum == CMAP["AUDIOLIB_CHANNEL_TOP_REAR_RIGHT"]:
                gain = targetGainR * backGain
            elif channel_enum == CMAP["AUDIOLIB_CHANNEL_OVERHEAD"]:
                gain = 1.0 * topMiddleGain

            final_channel_gains[i] = gain

        self.final_channel_gains = final_channel_gains

    def generate_inputs(
        self,
        test_type="SINE",
        num_blocks=1,
        sampling_rate=48000,
        amplitude=1.0,
        frequency=2000.0,
        f_start=None,
        f_end=None,
        channel_frequencies=None,
        min_val=-10,
        max_val=10,
    ):
        """Generate the planar test signal, shape (num_channels * num_blocks, num_samples) (test-data only).

        Block b is the rows [b * num_channels, (b + 1) * num_channels).
        """
        numSamples = self.num_samples
        numChannels = self.num_channels
        dtype = self.dtype
        CMAP = AUDIOLIB_channelMappings.AUDIOLIB_CHANNEL_MAP
        channel_frequencies = channel_frequencies or {}
        f_start = frequency if f_start is None else f_start
        f_end = frequency if f_end is None else f_end

        in0 = np.zeros((numChannels * num_blocks, numSamples), dtype=dtype)
        if test_type == "SINE":
            for i, channel_enum in enumerate(self.channel_config_integers):
                # Assign channel-specific frequency or default
                channel_freq = channel_frequencies.get(channel_enum, frequency + i * 50)
                in0[i, :] = self.generate_sine(
                    numSamples, sampling_rate, 1, i, channel_freq, amplitude, dtype
                )
        elif test_type == "SWEEP":
            for i in range(numChannels):
                in0[i, :] = self.generate_sweep(
                    numSamples, sampling_rate, 1, i, amplitude, f_start, f_end, dtype
                )
        else:  # STATIC
            in0 = np.random.uniform(
                low=min_val, high=max_val, size=(numChannels * num_blocks, numSamples)
            ).astype(dtype)
        return in0

    def exec(self, x):
        """Apply the per-channel gains to one frame.

        Args:
            x: interleaved [num_samples, num_channels] or planar [num_channels, num_samples].
        """
        x = np.asarray(x)
        if self.bypassMode:
            return x
        gains = self.final_channel_gains
        gains = gains if self.is_interleaved else gains[:, np.newaxis]
        return (x * gains).astype(self.dtype)
