# Copyright (C) 2026 Texas Instruments Incorporated
#
# SPDX-License-Identifier: LicenseRef-TI-TFL

import numpy as np


class MuteNCh:
    """Reference model of the muteNCh kernel (per-channel gain fade / mute)."""

    # fadeType enum key (as the kernel / master JSON passes it) -> fade label
    FADE_TYPES = {0: "LINEAR", 1: "SMOOTH", 2: "HARD"}

    def __init__(
        self,
        *,
        num_samples,
        num_channels,
        in_dtype,
        is_interleaved,
        samplingRate,
        fadeTime,
        fadeType,
        pIsMute,
        bypassMode=0,
        **_,
    ):
        self.num_samples = int(num_samples)
        self.num_channels = int(num_channels)
        self.dtype = np.dtype(in_dtype)
        self.is_interleaved = bool(is_interleaved)
        self.sampling_rate = samplingRate
        self.fade_time = fadeTime
        self.fade_type = self.FADE_TYPES[int(fadeType)]
        self.bypass_mode = bool(bypassMode)
        is_mute = np.asarray(pIsMute, dtype=np.float64).reshape(-1)
        if is_mute.size == 1:
            is_mute = np.repeat(is_mute, self.num_channels)
        # target gain 1 - mute; the fade starts from the opposite (muted) end
        self.target_gain = 1.0 - is_mute
        self.initial_gain = (is_mute == 1) * 1.0
        # one frame is num_samples samples per channel
        self.frame_size = self.num_samples
        self.init()

    def init(self):
        """Create the per-channel gain state from the initial gain (called by the constructor)."""
        self.reset()

    def reset(self):
        """(Re)create the per-channel gain state carried from frame to frame."""
        init = np.array(self.initial_gain)
        self.state = {
            "frame": 0,
            # initial gain, fixes the LINEAR fade step
            "start": init.copy(),
            # unrounded running gain used by the LINEAR fade
            "linear": init.copy(),
            # last output gain sample used by the SMOOTH fade
            "smooth": init.copy(),
        }
        # gains applied by the most recent exec() (num_channels, num_samples), for test-data dumps
        self.last_gain_frame = None

    @staticmethod
    def generate_sine(
        num_samples,
        sampling_rate,
        num_frames,
        channel,
        frequency,
        amplitude,
        num_channels=None,
        use_phase_offset=True,
    ):
        """Generate a continuous sine wave for all numBlocks of a given channel.

        Args:
            num_samples (int): Number of samples per frame.
            sampling_rate (int): Sampling rate in Hz.
            num_frames (int): Number of numBlocks.
            channel (int): Channel index (for phase offset if used).
            frequency (float): Frequency of the sine wave in Hz.
            amplitude (float): Amplitude of the sine wave.
            num_channels (int, optional): Number of channels (for phase offset).
            use_phase_offset (bool, optional): Whether to apply phase offset. Defaults to True.

        Returns:
            numpy.ndarray: Array of sine wave samples, shape (num_samples * num_frames,).
        """
        total_samples = num_samples * num_frames
        t = np.linspace(0, total_samples / sampling_rate, total_samples, endpoint=False)
        phase_offset = (
            (channel * np.pi / num_channels) if use_phase_offset and num_channels else 0.0
        )
        sine_wave = amplitude * np.sin(2 * np.pi * frequency * t + phase_offset)
        return sine_wave


    def compute_frame_gains(self):
        """Compute the per-sample gain ramp for ONE frame and advance the state.

        Returns:
            gains: float32 array of shape (num_channels, frame_size). The internal
            gain state is advanced to the next frame.
        """
        state = self.state
        targetGain = np.array(self.target_gain)
        numChannels = self.num_channels
        frameSize = self.frame_size
        samplingRate = self.sampling_rate
        fadeTime = self.fade_time
        fadeType = self.fade_type
        frame = state["frame"]

        pFrameGain = np.zeros((numChannels, frameSize), dtype=np.float32)
        gainThreshold = 1e-6

        for ch in range(numChannels):
            targ_g = targetGain[ch]
            if fadeType == "HARD":
                # Instantaneous gain change to target
                pFrameGain[ch, :] = targ_g
            elif fadeType == "LINEAR":
                curr_g = state["linear"][ch]
                timeConstantSec = fadeTime / 1000.0
                totalFadeSamples = timeConstantSec * samplingRate
                # the step is constant, derived from the initial gain
                gainStep = (
                    (targ_g - state["start"][ch]) / totalFadeSamples
                    if totalFadeSamples > 0 and fadeTime > 0
                    else 0
                )
                for i in range(frameSize):
                    if abs(curr_g - targ_g) < gainThreshold or fadeTime == 0:
                        curr_g = targ_g
                    else:
                        curr_g += gainStep
                        if (gainStep > 0 and curr_g > targ_g) or (
                            gainStep < 0 and curr_g < targ_g
                        ):
                            curr_g = targ_g
                    pFrameGain[ch, i] = curr_g
                state["linear"][ch] = curr_g
            elif fadeType == "SMOOTH":
                # Exponential smoothing
                prevGain = state["smooth"][ch]
                if frame == 0:
                    bypassSmoothing = fadeTime == 0.0
                else:
                    bypassSmoothing = abs(prevGain - targ_g) < gainThreshold
                if bypassSmoothing:
                    pFrameGain[ch, :] = targ_g
                else:
                    timeConstantSec = fadeTime / 1000.0
                    smoothingCoefficient = np.exp(-1.0 / (samplingRate * timeConstantSec))
                    for j in range(frameSize):
                        pFrameGain[ch, j] = (smoothingCoefficient * prevGain) + (
                            (1.0 - smoothingCoefficient) * targ_g
                        )
                        prevGain = pFrameGain[ch, j]
                state["smooth"][ch] = pFrameGain[ch, frameSize - 1]

        state["frame"] = frame + 1
        return pFrameGain


    def generate_inputs(
        self,
        test_type="STATIC",
        num_blocks=1,
        frequency=2000.0,
        amplitude=1.0,
        min_val=-10,
        max_val=10,
    ):
        """Generate the input samples (STATIC random or SINE) for all blocks (test-data only).

        Returns:
            numpy.ndarray: float32 array of shape (num_blocks, num_samples * num_channels).
        """
        testType = test_type
        isInterleaved = self.is_interleaved
        numSamples = self.num_samples
        numChannels = self.num_channels
        numBlocks = num_blocks
        samplingRate = self.sampling_rate
        minVal = min_val
        maxVal = max_val
        samplesPerFrame = numSamples * numChannels

        pInLocal = np.zeros((numBlocks, samplesPerFrame), dtype=np.float32)
        if testType == "STATIC":
            if isInterleaved:
                for frame in range(numBlocks):
                    for j in range(numSamples):
                        for c in range(numChannels):
                            idx = j * numChannels + c
                            pInLocal[frame, idx] = np.random.uniform(
                                low=minVal, high=maxVal
                            )
            else:
                for frame in range(numBlocks):
                    for i in range(numChannels):
                        startIdx = i * numSamples
                        endIdx = (i + 1) * numSamples
                        pInLocal[frame, startIdx:endIdx] = np.random.uniform(
                            low=minVal, high=maxVal, size=numSamples
                        ).astype(np.float32)
        elif testType == "SINE":
            if isInterleaved:
                for frame in range(numBlocks):
                    for j in range(numSamples):
                        for c in range(numChannels):
                            idx = j * numChannels + c
                            frame_start = frame * numSamples
                            sine_wave = self.generate_sine(
                                numSamples,
                                samplingRate,
                                numBlocks,
                                c,
                                frequency,
                                amplitude,
                                numChannels,
                                use_phase_offset=True,
                            )
                            pInLocal[frame, idx] = sine_wave[frame_start + j]
            else:
                for i in range(numChannels):
                    startIdx = i * numSamples
                    endIdx = (i + 1) * numSamples
                    sine_wave = self.generate_sine(
                        numSamples,
                        samplingRate,
                        numBlocks,
                        channel=i,
                        frequency=frequency,
                        amplitude=amplitude,
                        num_channels=numChannels,
                        use_phase_offset=True,
                    )
                    for frame in range(numBlocks):
                        frame_start = frame * numSamples
                        frame_end = (frame + 1) * numSamples
                        pInLocal[frame, startIdx:endIdx] = sine_wave[frame_start:frame_end]

        return pInLocal


    def apply_frame_gains(self, in_frame, gain_frame):
        """Apply one frame of gains to one frame of input; bypass copies input to output.

        in_frame has shape (num_samples * num_channels,), gain_frame has shape
        (num_channels, num_samples).
        """
        num_samples = self.num_samples
        num_channels = self.num_channels
        is_interleaved = self.is_interleaved

        if self.bypass_mode:
            return in_frame.copy()

        pOut = np.zeros(num_samples * num_channels, dtype=self.dtype)
        if is_interleaved:
            for j in range(num_samples):
                for c in range(num_channels):
                    idx = j * num_channels + c
                    pOut[idx] = in_frame[idx] * gain_frame[c, j]
        else:
            for i in range(num_channels):
                startIdx = i * num_samples
                for j in range(num_samples):
                    pOut[startIdx + j] = in_frame[startIdx + j] * gain_frame[i, j]
        return pOut

    def exec(self, x):
        """Process one frame: advance the fade state and apply the gains.

        Args:
            x: one input frame in the kernel layout, interleaved [num_samples, num_channels]
               or planar [num_channels, num_samples] (a flat frame is accepted too).

        Returns:
            the output frame, same shape and dtype as x. The gains applied are kept in
            self.last_gain_frame, shape (num_channels, num_samples).
        """
        x = np.asarray(x)
        gain_frame = self.compute_frame_gains()
        self.last_gain_frame = gain_frame
        out = self.apply_frame_gains(x.reshape(-1), gain_frame)
        return out.astype(self.dtype).reshape(x.shape)
