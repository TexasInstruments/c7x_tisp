# Copyright (C) 2026 Texas Instruments Incorporated
#
# SPDX-License-Identifier: LicenseRef-TI-TFL

import numpy as np


class MuteNCh:
    """Reference model of the muteNCh kernel (per-channel gain fade / mute).

    Same per-fade gain formulas as AUDIOLIB_muteNCh, run one frame per exec() with the gain state carried from frame
    to frame. Two ways to build it:

    MuteNCh(**operator parameters)  as the node is built from the descriptor (TVM pyeval): num_samples,
                                    num_channels, in_dtype, is_interleaved, samplingRate, fadeTime, fadeType, pIsMute,
                                    bypassMode. The gains start where the kernel leaves them after init() + set().
    MuteNCh.from_gains(...)         AUDIOLIB_muteNCh's arguments with explicit initialGain / targetGain, for the
                                    TISP_muteNCh and TISP_isolateNCh generators and their supernodes.

    Both take keyword arguments by name only: a misspelt or unknown name raises TypeError instead of falling back to
    a default.
    """

    # fadeType enum value (AUDIOLIB_MUTE_FADE_TYPE_*, as the descriptor and the kernel pass it) -> fade label
    FADE_TYPES = {0: "LINEAR", 1: "SMOOTH", 2: "HARD", 3: "SYMMETRIC"}

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
    ):
        """Build the model from the operator's parameters, as the node is built from the descriptor.

        After AUDIOLIB_muteNCh_init() every channel's gain is 0, and set() keeps it and sets the target to 1 - pIsMute,
        so each channel fades from 0 to its target.

        Args:
            num_samples (int): Number of samples per channel per frame
            num_channels (int): Number of channels
            in_dtype (str or dtype): Data type, e.g., "float32"
            is_interleaved (int): 1 interleaved [samples, channels], 0 planar [channels, samples]
            samplingRate (int): Sampling rate in Hz
            fadeTime (float): Fade time in milliseconds
            fadeType (int or str): Enum value (0 LINEAR, 1 SMOOTH, 2 HARD, 3 SYMMETRIC) or its label
            pIsMute (scalar or array): Mute flag per channel, 1 mutes (target gain 0), 0 plays (target gain 1); a
                scalar applies to every channel
            bypassMode (int): 1 copies the input to the output
        """
        numChannels = int(num_channels)
        if isinstance(fadeType, str):
            fadeTypeLabel = fadeType.upper()
            if fadeTypeLabel not in self.FADE_TYPES.values():
                raise ValueError(f"fadeType must be one of {sorted(self.FADE_TYPES.values())}, got {fadeType!r}")
        else:
            if int(fadeType) not in self.FADE_TYPES:
                raise ValueError(f"fadeType must be one of {sorted(self.FADE_TYPES)}, got {fadeType!r}")
            fadeTypeLabel = self.FADE_TYPES[int(fadeType)]
        isMute = np.asarray(pIsMute).reshape(-1)
        if isMute.size == 1:
            isMute = np.repeat(isMute, numChannels)
        if isMute.size != numChannels:
            raise ValueError(f"pIsMute has {isMute.size} flags, expected 1 or num_channels = {numChannels}")
        if not np.isin(isMute, (0, 1)).all():
            raise ValueError(f"pIsMute flags must be 0 or 1, got {isMute.tolist()}")
        self._setup(
            dType=np.dtype(str(in_dtype)).name,
            numChannels=numChannels,
            numSamples=int(num_samples),
            samplingRate=int(samplingRate),
            fadeTime=float(fadeTime),
            fadeType=fadeTypeLabel,
            initialGain=[0.0] * numChannels,
            targetGain=[1.0 - float(m) for m in isMute],
            isInterleaved=bool(is_interleaved),
            bypassMode=bypassMode,
        )

    @classmethod
    def from_gains(
        cls,
        dType="float32",
        numChannels=1,
        numSamples=1,
        samplingRate=48000,
        fadeTime=0.0,
        fadeType="HARD",
        initialGain=None,
        targetGain=None,
        isInterleaved=False,
        bypassMode=0,
    ):
        """Build the model from AUDIOLIB_muteNCh's arguments with explicit gains (the generators' form).

        Args:
            dType (str): Data type, e.g., "float32", "float64"
            numChannels (int): Number of channels
            numSamples (int): Number of samples per frame
            samplingRate (int): Sampling rate in Hz
            fadeTime (float): Fade time in milliseconds
            fadeType (str): Type of fade ("HARD", "LINEAR", "SMOOTH", "SYMMETRIC")
            initialGain (list): Initial gain values for each channel
            targetGain (list): Target gain values for each channel
            isInterleaved (bool): Whether the data is interleaved
            bypassMode (int): 1 copies the input to the output
        """
        self = cls.__new__(cls)
        self._setup(
            dType=dType,
            numChannels=numChannels,
            numSamples=numSamples,
            samplingRate=samplingRate,
            fadeTime=fadeTime,
            fadeType=fadeType,
            initialGain=initialGain,
            targetGain=targetGain,
            isInterleaved=isInterleaved,
            bypassMode=bypassMode,
        )
        return self

    def _setup(
        self,
        *,
        dType,
        numChannels,
        numSamples,
        samplingRate,
        fadeTime,
        fadeType,
        initialGain,
        targetGain,
        isInterleaved,
        bypassMode,
    ):
        """Store the private members and create the gain state (shared by both constructors)."""
        self._dType = dType
        self._numChannels = numChannels
        self._numSamples = numSamples
        self._samplingRate = samplingRate
        self._fadeTime = fadeTime
        self._fadeType = fadeType
        self._initialGain = (
            initialGain if initialGain is not None else [1.0] * numChannels
        )
        self._targetGain = targetGain if targetGain is not None else [0.0] * numChannels
        self._isInterleaved = isInterleaved
        self._bypassMode = bool(bypassMode)
        self.init()

    def init(self):
        """Create the per-channel gain state carried from frame to frame (called by the constructor)."""
        self._frame = 0
        # LINEAR: unrounded running gain; SYMMETRIC: running gain
        self._currentGain = np.array(self._initialGain, dtype=np.float64)
        # SMOOTH: distance from the target gain, carried in float32 as the kernel's smoothDistance
        self._smoothDistance = np.float32(self._initialGain) - np.float32(self._targetGain)
        # gains applied by the most recent exec() (numChannels, numSamples), for test-data dumps
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

    def _calculate_gain(self):
        """Calculate one frame of gain values for each channel based on fade type and advance the gain state.

        Returns:
            numpy.ndarray: Gain array of shape (numChannels, numSamples).
        """
        frameSize = self._numSamples
        pCurrentGain = np.zeros((self._numChannels, frameSize), dtype=np.float32)
        gainThreshold = 1e-6

        # Compute gains based on fadeType for each channel
        for ch in range(self._numChannels):
            curr_g = self._currentGain[ch]
            targ_g = self._targetGain[ch]
            if self._fadeType == "HARD" or self._fadeTime == 0:
                # Instantaneous gain change to target
                pCurrentGain[ch, :] = targ_g
            elif self._fadeType == "LINEAR":
                # Linear fade at a constant rate: a full fade, from 1 - target to target, lasts fadeTime
                timeConstantSec = self._fadeTime / 1000.0
                totalFadeSamples = timeConstantSec * self._samplingRate
                gainStep = (targ_g - (1.0 - targ_g)) / totalFadeSamples
                for i in range(frameSize):
                    if abs(curr_g - targ_g) < gainThreshold:
                        curr_g = targ_g
                    else:
                        curr_g += gainStep
                        if (gainStep > 0 and curr_g > targ_g) or (
                            gainStep < 0 and curr_g < targ_g
                        ):
                            curr_g = targ_g
                    pCurrentGain[ch, i] = curr_g
            elif self._fadeType == "SYMMETRIC":
                # Constant ratio per sample between the floor (-200 dB) and 1, in float32 as the kernel computes it
                floor = np.float32(1e-10)
                totalFadeSamples = (
                    np.float32(self._fadeTime) * np.float32(self._samplingRate)
                ) / np.float32(1000.0)
                ratioUp = np.float32(
                    np.power(np.float32(1.0) / floor, np.float32(1.0) / totalFadeSamples)
                )
                ratio = ratioUp if targ_g == 1.0 else np.float32(np.float32(1.0) / ratioUp)
                curr_g = np.float32(curr_g)
                if self._frame == 0:
                    # set() starts the ramp from the running gain, no lower than one ratio step below the floor
                    curr_g = max(curr_g, np.float32(floor / ratio))
                for i in range(frameSize):
                    curr_g = min(max(np.float32(curr_g * ratio), floor), np.float32(1.0))
                    pCurrentGain[ch, i] = curr_g if curr_g > floor else 0.0
            elif self._fadeType == "SMOOTH":
                # Exponential smoothing on the distance from the target, in float32 as the kernel computes it:
                # the distance shrinks by alpha every sample and the gain is target + distance
                timeConstantSec = np.float32(self._fadeTime) / np.float32(1000.0)
                smoothingCoefficient = np.float32(
                    np.exp(np.float32(-1.0) / (np.float32(self._samplingRate) * timeConstantSec))
                )
                targ_g = np.float32(targ_g)
                distance = self._smoothDistance[ch]
                for i in range(frameSize):
                    distance = np.float32(distance * smoothingCoefficient)
                    curr_g = np.float32(targ_g + distance)
                    pCurrentGain[ch, i] = curr_g
                self._smoothDistance[ch] = distance
            self._currentGain[ch] = curr_g

        self._frame += 1
        return pCurrentGain

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
        isInterleaved = self._isInterleaved
        numSamples = self._numSamples
        numChannels = self._numChannels
        numBlocks = num_blocks
        samplingRate = self._samplingRate
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

    def _apply_gain(self, pInLocal, pCurrentGain):
        """Apply one frame of gain values to one frame of input data to generate output data.

        Args:
            pInLocal (numpy.ndarray): Input data array of shape (numSamples * numChannels,).
            pCurrentGain (numpy.ndarray): Gain array of shape (numChannels, numSamples).

        Returns:
            numpy.ndarray: Output data array of shape (numSamples * numChannels,).
        """
        pOutLocal = np.zeros(self._numSamples * self._numChannels, dtype=self._dType)

        if self._isInterleaved:
            for j in range(self._numSamples):
                for c in range(self._numChannels):
                    idx = j * self._numChannels + c
                    pOutLocal[idx] = pInLocal[idx] * pCurrentGain[c, j]
        else:
            for i in range(self._numChannels):
                startIdx = i * self._numSamples
                for j in range(self._numSamples):
                    pOutLocal[startIdx + j] = (
                        pInLocal[startIdx + j] * pCurrentGain[i, j]
                    )

        return pOutLocal

    def exec(self, pInLocal):
        """Execute the muteNCh algorithm on one frame: advance the fade state and apply the gains.

        Args:
            pInLocal (numpy.ndarray): One input frame in the kernel layout, interleaved [numSamples, numChannels]
                or planar [numChannels, numSamples] (a flat frame is accepted too).

        Returns:
            numpy.ndarray: The output frame, same shape as pInLocal; bypassMode copies the input. The gains
            applied are kept in self.last_gain_frame, shape (numChannels, numSamples).
        """
        pInLocal = np.asarray(pInLocal)
        # Calculate gain values
        pCurrentGain = self._calculate_gain()
        self.last_gain_frame = pCurrentGain

        if self._bypassMode:
            pOutLocal = pInLocal.reshape(-1).copy()
        else:
            # Apply gain to input data
            pOutLocal = self._apply_gain(pInLocal.reshape(-1), pCurrentGain)

        return pOutLocal.astype(self._dType).reshape(pInLocal.shape)
