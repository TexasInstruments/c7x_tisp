// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#include <cstddef>
#include <cstdint>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "TISP_limiter.hpp"

/**
 *  @brief Zeroes the natural-C reference's gain-smoothing state.
 *
 *  @details The `_cn` counterpart of `Limiter::cleanInternalBuffers()`. Call it once before the
 *           first `TISP_limiter_exec_cn` of a stream; the exec carries the envelope forward from
 *           there, so calling it between blocks would discard the state the reference exists to
 *           model.
 *
 *  @param [out] pGainState  : Per-channel gain-smoothing state, numChannels elements
 *  @param [in]  numChannels : Number of channels
 *  @return None
 */
template <typename dataType> void TISP_limiter_init_cn(void *pGainState, size_t numChannels);

/**
 *  @brief Natural-C reference for the TISP limiter node.
 *
 *  @param [in,out] pGainState : Per-channel gain-smoothing state, numChannels elements. Holds
 *                               the smoothed gain WITHOUT the make-up gain, matching the
 *                               kernel's pGainState. Persists across calls; zero it before the
 *                               first call.
 *  @remarks pOut must not alias pIn - pOut is reused as scratch for the absolute values and
 *           then for the gain in dB, so an aliased pIn would be destroyed before the final
 *           multiply reads it.
 */
template <typename dataType>
void TISP_limiter_exec_cn(void  *pIn,
                          void  *pGainState,
                          void  *pScratch,
                          void  *pOut,
                          void  *pThreshold_dB,
                          void  *pKneeWidth_dB,
                          void  *pAttackCoeff,
                          void  *pReleaseCoeff,
                          size_t numChannels,
                          size_t blockSize,
                          size_t pitchX,
                          size_t pitchS,
                          size_t pitchY);
