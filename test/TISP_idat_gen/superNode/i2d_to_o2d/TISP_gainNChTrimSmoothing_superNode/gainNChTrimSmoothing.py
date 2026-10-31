# Copyright (C) 2026 Texas Instruments Incorporated
#
# SPDX-License-Identifier: Apache-2.0

import numpy as np


class AUDIOLIB_gainNChTrimSmoothing:
    """Per-channel smoothed gain with two fade shapes, selected by fadeType.

    The kernel API is modelled as set() (called whenever the user changes a gain) followed
    by exec() on every block. The per-channel target is masterGain * trimGain[ch].

    fadeType "SYMMETRIC":
        fadeTime is the ramp duration in ms. The gain moves by a constant ratio per sample
        (a straight line in dB), so fades up and down take the same time. Each sample is
        scaled by the current gain before it is multiplied by the step, and after
        N = int(fadeTime * fs / 1000) samples the gain snaps to the target exactly.

    fadeType "SMOOTH":
        fadeTime is the one-pole time constant in ms. Each sample first updates
        gain = gain * (1 - coeff) + target * coeff, with coeff = 1 - exp(-1 / (fs * T)),
        and then scales the sample. The gain approaches the target and never snaps.

    fadeTime = 0 applies the target from the first sample in both modes.
    """

    FADE_TYPES = ("SYMMETRIC", "SMOOTH")
    # Linear gain 0 is treated as -200 dB in the SYMMETRIC ratio so the ramp has a finite step.
    GAIN_FLOOR = 1.0e-10

    def __init__(self, dType="float", inChannels=1, inSamples=1, samplingRate=48000, fadeTime=0.0,
                 fadeType="SYMMETRIC", isInterleaved=0, numExecReps=1):
        """Initialize the AUDIOLIB_gainNChTrimSmoothing class with private members.

        Args:
            dType (str): Data type, e.g., "float", "double"
            inChannels (int): Number of input channels
            inSamples (int): Number of samples per channel per frame
            samplingRate (float): Sampling rate in Hz
            fadeTime (float): SYMMETRIC: ramp duration in ms. SMOOTH: time constant in ms
            fadeType (str): "SYMMETRIC" or "SMOOTH"
            isInterleaved (int): Whether data is interleaved (1) or planar (0)
            numExecReps (int): Number of frames processed
        """
        self._dType = dType.lower()
        self._npDtype = np.float64 if self._dType == "double" else np.float32
        self._samplingRate = float(samplingRate)
        self._fadeTime = float(fadeTime)
        self._fadeType = fadeType.upper()
        if self._fadeType not in self.FADE_TYPES:
            raise ValueError(f"fadeType must be one of {self.FADE_TYPES}, got {fadeType}")
        self._numExecReps = int(numExecReps)

        self._inChannels = int(inChannels)
        self._inSamples = int(inSamples)
        self._isInterleaved = int(isInterleaved)

        dtype = self._npDtype
        # Internal state tracking per channel across frames
        self._currentGain = np.ones(self._inChannels, dtype=dtype)
        self._targetGain = np.ones(self._inChannels, dtype=dtype)
        self._gainStep = np.ones(self._inChannels, dtype=dtype)
        self._remainingFadeSamples = np.zeros(self._inChannels, dtype=np.int64)

        # Derived once from fadeTime, as the kernel's init/set would.
        # SYMMETRIC: fade length in samples, computed in the kernel's precision and truncated.
        self._totalFadeSamples = int((dtype(self._fadeTime) / dtype(1000.0)) * dtype(self._samplingRate))
        # SMOOTH: one-pole coefficient; fadeTime 0 gives coeff 1, i.e. the target immediately.
        if self._fadeTime > 0.0:
            self._smoothingCoeff = dtype(1.0 - np.exp(-1.0 / (self._samplingRate * self._fadeTime / 1000.0)))
        else:
            self._smoothingCoeff = dtype(1.0)

    def set(self, initialGain, trimGain, masterGain=1.0):
        """Load new per-channel gains, as one set() call from a user interface would.

        Args:
            initialGain (array or None): Per-channel gain applied now; seeds the running gain
                                 with the channel at rest. It already includes masterGain, so
                                 it is not multiplied by it again. None keeps the running gain,
                                 as every later set() from a user interface does.
            trimGain (array): Per-channel gain setting; the target is masterGain * trimGain[ch]
            masterGain (float): Gain applied on top of every channel's trimGain
        """
        dtype = self._npDtype
        for ch in range(self._inChannels):
            if initialGain is not None:
                self._currentGain[ch] = dtype(initialGain[ch])
                self._targetGain[ch] = dtype(initialGain[ch])
                self._gainStep[ch] = dtype(1.0)
                self._remainingFadeSamples[ch] = 0
            target = dtype(dtype(masterGain) * dtype(trimGain[ch]))

            if self._fadeType == "SMOOTH":
                self._targetGain[ch] = target
                continue

            # Compare with the previous target, not the running gain: mid-fade the running
            # gain never equals the target, and a channel whose gains did not change must
            # keep its fade instead of restarting it with a full fadeTime.
            changed = not np.isclose(target, self._targetGain[ch], rtol=1e-6)
            if not changed and self._remainingFadeSamples[ch] > 0:
                continue
            if changed and self._totalFadeSamples > 0:
                floor = dtype(self.GAIN_FLOOR)
                start = max(dtype(self._currentGain[ch]), floor)
                ratio = dtype(max(target, floor) / start)
                self._currentGain[ch] = start
                self._gainStep[ch] = dtype(np.power(ratio, dtype(1.0) / dtype(self._totalFadeSamples)))
                self._remainingFadeSamples[ch] = self._totalFadeSamples
            else:
                self._currentGain[ch] = target
                self._gainStep[ch] = dtype(1.0)
                self._remainingFadeSamples[ch] = 0
            self._targetGain[ch] = target

    def _symmetricGain(self, ch, numSamples):
        """Gain curve for one frame of one channel, preserving fade progress (SYMMETRIC)."""
        dtype = self._npDtype
        gainCurve = np.empty(numSamples, dtype=dtype)
        gain = dtype(self._currentGain[ch])
        targetGain = dtype(self._targetGain[ch])
        gainStep = dtype(self._gainStep[ch])
        remaining = int(self._remainingFadeSamples[ch])

        for s in range(numSamples):
            if remaining > 0:
                gainCurve[s] = gain
                gain = dtype(gain * gainStep)
                remaining -= 1
                if remaining == 0:
                    gain = targetGain
            else:
                gainCurve[s] = targetGain
                gain = targetGain

        self._currentGain[ch] = gain
        self._remainingFadeSamples[ch] = remaining
        return gainCurve

    def _smoothGain(self, ch, numSamples):
        """Gain curve for one frame of one channel, preserving the smoother state (SMOOTH)."""
        dtype = self._npDtype
        gainCurve = np.empty(numSamples, dtype=dtype)
        gain = dtype(self._currentGain[ch])
        oneMinusCoeff = dtype(dtype(1.0) - self._smoothingCoeff)
        targetTimesCoeff = dtype(self._targetGain[ch] * self._smoothingCoeff)

        for s in range(numSamples):
            gain = dtype(dtype(gain * oneMinusCoeff) + targetTimesCoeff)
            gainCurve[s] = gain

        self._currentGain[ch] = gain
        return gainCurve

    def exec(self, in0):
        """Process frames of audio with the smoothed per-channel gains.

        Args:
            in0 (np.ndarray): [numFrames, numSamples, numChannels] when interleaved,
                              [numFrames, numChannels, numSamples] when planar

        Returns:
            (np.ndarray, np.ndarray): output in the input's layout, and the gain applied to
            every sample as [numFrames, numChannels, numSamples]
        """
        dtype = self._npDtype
        in0 = np.asarray(in0, dtype=dtype)
        out = np.empty_like(in0)

        if self._isInterleaved:
            numFrames, numSamples, numChannels = in0.shape
        else:
            numFrames, numChannels, numSamples = in0.shape
        gains = np.empty((numFrames, numChannels, numSamples), dtype=dtype)
        computeGain = self._smoothGain if self._fadeType == "SMOOTH" else self._symmetricGain

        for frame in range(numFrames):
            for ch in range(numChannels):
                gainCurve = computeGain(ch, numSamples)
                gains[frame, ch] = gainCurve

                if self._isInterleaved:
                    out[frame, :, ch] = in0[frame, :, ch] * gainCurve
                else:
                    out[frame, ch, :] = in0[frame, ch, :] * gainCurve

        return out, gains
