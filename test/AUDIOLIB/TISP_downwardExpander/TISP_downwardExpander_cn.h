// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include <cstddef>
#include <cstdint>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "TISP_downwardExpander.hpp"

/**
 *  @brief Zeroes the natural-C reference's gain-smoothing and hold state.
 *
 *  @details The `_cn` counterpart of `DownwardExpander::cleanInternalBuffers()`. Call it once before the
 *           first `TISP_downwardExpander_exec_cn` of a stream; the exec carries the envelope and hold
 *           counter forward from there, so calling it between blocks would discard the state the
 *           reference exists to model.
 *
 *  @param [out] pGainState  : Per-channel gain-smoothing state, numChannels elements
 *  @param [out] pHoldState  : Per-channel hold counter state, numChannels elements
 *  @param [in]  numChannels : Number of channels
 *  @return None
 */
template <typename dataType> void TISP_downwardExpander_init_cn(void *pGainState, void *pHoldState, size_t numChannels);

/**
 *  @brief Natural-C reference for the TISP expander node.
 *
 *  @param [in,out] pGainState : Per-channel gain-smoothing state, numChannels elements, matching
 *                               the kernel's pGainState. Persists across calls; zero it before the
 *                               first call.
 *  @param [in,out] pHoldState : Per-channel hold counter state, numChannels elements, matching the
 *                               kernel's pHoldState. Persists across calls; zero it before the
 *                               first call.
 *  @remarks pOut must not alias pIn - pOut is reused as scratch for the absolute values and
 *           then for the gain in dB, so an aliased pIn would be destroyed before the final
 *           multiply reads it.
 */
template <typename dataType>
void TISP_downwardExpander_exec_cn(void  *pIn,
                           void  *pGainState,
                           void  *pHoldState,
                           void  *pScratch,
                           void  *pOut,
                           void  *pThreshold_dB,
                           void  *pRatio,
                           void  *pKneeWidth_dB,
                           void  *pAttackCoeff,
                           void  *pReleaseCoeff,
                           void  *pHoldSamples,
                           size_t numChannels,
                           size_t blockSize,
                           size_t pitchX,
                           size_t pitchS,
                           size_t pitchY);
