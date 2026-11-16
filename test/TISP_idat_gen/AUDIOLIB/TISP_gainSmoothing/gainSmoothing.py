# Copyright (C) 2026 Texas Instruments Incorporated
#
# SPDX-License-Identifier: LicenseRef-TI-TFL

import numpy as np


class GainSmoothing:
    """Reference model of the gainSmoothing kernel: applies a gain ramp across audio frames
    with strict dtype enforcement."""
    
    def __init__(self, *, dType="float", inSamples=1, inChannels=1, currentGain=1.0,
                 targetGain=1.0, samplingRate=48000.0, fadeTime=0.0, isInterleaved=False,
                 fadeType="SYMMETRIC", numExecReps=1, bypassMode=0, **_):
        self._dType = dType.lower()
        self._inSamples = inSamples
        self._inChannels = inChannels
        self._currentGain = currentGain
        self._targetGain = targetGain
        self._samplingRate = samplingRate
        self._fadeTime = fadeTime
        self._isInterleaved = isInterleaved
        self._fadeType = fadeType
        self._numExecReps = numExecReps
        self._bypassMode = bypassMode
        self.init()

    def init(self):
        self._npDtype = np.float64 if self._dType == "double" else np.float32

        self._inSamples = int(self._inSamples)
        self._inChannels = int(self._inChannels)
        self._currentGain = float(self._currentGain)
        self._targetGain = float(self._targetGain)
        self._samplingRate = float(self._samplingRate)
        self._fadeTime = float(self._fadeTime)
        self._isInterleaved = bool(self._isInterleaved)
        self._fadeType = self._fadeType.upper()
        self._numExecReps = int(self._numExecReps)

        self._frameSize = self._inSamples
        self._samplesPerFrame = self._inSamples * self._inChannels
        self._totalSamplesPerChannel = self._inSamples * self._numExecReps

    def generate_inputs(self, *, shape, min_val=-10, max_val=10):
        """Uniform test data (float32) of the given shape."""
        return np.random.uniform(low=min_val, high=max_val, size=shape).astype(
            self._npDtype if self._dType == "double" else "float32"
        )

    def _compute_symmetric_gain(self) -> np.ndarray:
        dtype = self._npDtype
        targetGain = dtype(self._targetGain)
        currentGain = dtype(self._currentGain)
        total_samples = self._totalSamplesPerChannel

        if currentGain <= dtype(0.0) or targetGain <= dtype(0.0):
            raise ValueError("Symmetric fading requires positive gains.")

        timeConstantSec = dtype(self._fadeTime / 1000.0)
        totalFadeSamples = dtype(timeConstantSec * dtype(self._samplingRate))

        pCurrentGain = np.full(total_samples, targetGain, dtype=dtype)

        if totalFadeSamples > dtype(0.0) and self._fadeTime > 0:
            fade_len = min(int(totalFadeSamples), total_samples)

            gainRatio = dtype(targetGain / currentGain)
            invSamples = dtype(dtype(1.0) / totalFadeSamples)
            gainStep = dtype(np.power(gainRatio, invSamples))

            gainCurve = np.empty(fade_len, dtype=dtype)
            accumGain = currentGain

            for s in range(fade_len):
                accumGain = dtype(accumGain * gainStep)
                gainCurve[s] = accumGain

            if gainStep > dtype(1.0):
                gainCurve = np.minimum(gainCurve, targetGain)
            else:
                gainCurve = np.maximum(gainCurve, targetGain)

            pCurrentGain[:fade_len] = gainCurve
            pCurrentGain[fade_len:] = targetGain

            
        return pCurrentGain

    def _compute_smooth_gain(self) -> np.ndarray:
        targetGain = self._npDtype(self._targetGain)
        total_samples = self._totalSamplesPerChannel
        pCurrentGain = np.zeros(total_samples, dtype=self._npDtype)
        gainThreshold = self._npDtype(1e-6)

        timeConstantSec = self._fadeTime / 1000.0
        
        if self._fadeTime <= 0.0:
            pCurrentGain[:] = targetGain
            return pCurrentGain

        alpha = self._npDtype(np.exp(-1.0 / (self._samplingRate * timeConstantSec)))
        one_minus_alpha = self._npDtype(1.0) - alpha
        
        currentGain = self._npDtype(self._currentGain)
        for frame in range(self._numExecReps):
            startIdx = frame * self._frameSize
            endIdx = startIdx + self._frameSize

            if frame > 0 and abs(pCurrentGain[startIdx - 1] - targetGain) < gainThreshold:
                pCurrentGain[startIdx:endIdx] = targetGain
                currentGain = targetGain
            else:
                for j in range(startIdx, endIdx):
                    currentGain = alpha * currentGain + one_minus_alpha * targetGain
                    pCurrentGain[j] = currentGain

        return pCurrentGain

    def exec(self, pInLocal: np.ndarray):
        # bypass copies the input to the output, leaving the gain unapplied
        if self._bypassMode:
            return np.array(pInLocal, copy=True), None

        pInLocal = np.asarray(pInLocal, dtype=self._npDtype)
        
        if self._fadeType == "SYMMETRIC":
            pCurrentGain = self._compute_symmetric_gain()
        elif self._fadeType == "SMOOTH":
            pCurrentGain = self._compute_smooth_gain()
        else:
            pCurrentGain = np.full(self._totalSamplesPerChannel, self._targetGain, dtype=self._npDtype)

        gain_per_frame = pCurrentGain.reshape(self._numExecReps, self._inSamples)

        if self._isInterleaved:
            gain_expanded = np.repeat(gain_per_frame, self._inChannels, axis=1)
        else:
            gain_expanded = np.tile(gain_per_frame, (1, self._inChannels))

        pInLocal_2d = pInLocal.reshape(self._numExecReps, self._samplesPerFrame)
        pOutLocal = pInLocal_2d * gain_expanded

        # Return NumPy arrays directly (flattened) to preserve exact float32 dtype
        return pOutLocal, pCurrentGain
