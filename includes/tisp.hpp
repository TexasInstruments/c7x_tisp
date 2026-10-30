// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

#pragma once

#include "../src/FFTLIB/TISP_twiddle_lut.hpp"
#include "TISP_absoluteValue.hpp"
#include "TISP_acos.hpp"
#include "TISP_addConstant.hpp"
#include "TISP_addNCh.hpp"
#include "TISP_asin.hpp"
#include "TISP_asrc.hpp"
#include "TISP_atan.hpp"
#include "TISP_balance.hpp"
#include "TISP_balanceFader.hpp"
#include "TISP_binaryLogicGate.hpp"
#include "TISP_bitwiseANDConst.hpp"
#include "TISP_blockCopy.hpp"
#include "TISP_blockStatistics.hpp"
#include "TISP_cascadeBiquad.hpp"
#include "TISP_ceil.hpp"
#include "TISP_common.hpp"
#include "TISP_concat.hpp"
#include "TISP_conditionalReplace.hpp"
#include "TISP_cos.hpp"
#include "TISP_cosh.hpp"
#include "TISP_dB10.hpp"
#include "TISP_dB20.hpp"
#include "TISP_dataConvert.hpp"
#include "TISP_dcSource.hpp"
#include "TISP_deinterleave.hpp"
#include "TISP_delay.hpp"
#include "TISP_delayDru.hpp"
#include "TISP_delayNChannel.hpp"
#include "TISP_div.hpp"
#include "TISP_exp.hpp"
#include "TISP_fft1dBatched.hpp"
#include "TISP_fir.hpp"
#include "TISP_floor.hpp"
#include "TISP_gain.hpp"
#include "TISP_gainNCh.hpp"
#include "TISP_gainNChTrim.hpp"
#include "TISP_ifftr1dBatched.hpp"
#include "TISP_initDru.hpp"
#include "TISP_interleave.hpp"
#include "TISP_istft.hpp"
#include "TISP_limiter.hpp"
#include "TISP_log.hpp"
#include "TISP_logicalNOT.hpp"
#include "TISP_logMelSpectrum.hpp"
#include "TISP_log10.hpp"
#include "TISP_log2.hpp"
#include "TISP_matMul.hpp"
#include "TISP_matTrans.hpp"
#include "TISP_multiplexer.hpp"
#include "TISP_multiplexerSmooth.hpp"
#include "TISP_mute.hpp"
#include "TISP_muteNCh.hpp"
#include "TISP_nlms.hpp"
#include "TISP_nonZeroCheck.hpp"
#include "TISP_nullSink.hpp"
#include "TISP_peakAbsoluteValue.hpp"
#include "TISP_realImagToComplex.hpp"
#include "TISP_recip.hpp"
#include "TISP_rfft1dBatched.hpp"
#include "TISP_router.hpp"
#include "TISP_sin.hpp"
#include "TISP_sinh.hpp"
#include "TISP_sinusoidGenerator.hpp"
#include "TISP_softClip.hpp"
#include "TISP_split.hpp"
#include "TISP_sqr.hpp"
#include "TISP_sqrt.hpp"
#include "TISP_ssrc.hpp"
#include "TISP_stft.hpp"
#include "TISP_subBlockStatistics.hpp"
#include "TISP_superNode.hpp"
#include "TISP_tableInterpolate.hpp"
#include "TISP_tableLookup.hpp"
#include "TISP_tan.hpp"
#include "TISP_tanh.hpp"
#include "TISP_thresholdDetector.hpp"
#include "TISP_typeConversion.hpp"
#include "TISP_undB20.hpp"
#include "TISP_util.hpp"
#include "TISP_whiteNoise.hpp"

/**
 * \defgroup NODE Nodes
 * \brief Node-level APIs
 */

/**
 *  @ingroup NODE
 */

/**@{*/

/**
 * \defgroup audiolibNode AUDIOLIB Nodes
 * \brief Node-level APIs supported from AUDIOLIB.
 */

/**
 * \defgroup dsplibNode DSPLIB Nodes
 * \brief Node-level APIs supported from DSPLIB.
 */

/**
 * \defgroup miscNode Miscellaneous Nodes
 * \brief Node-level APIs supported for miscellaneous kernels implemented within TISP, e.g., block copy kernel.
 */

/**
 * \defgroup fftlibNode FFTLIB Nodes
 * \brief Node-level APIs supported for FFT-style kernels from FFTLIB
 * \details Currently, we support only one kernel. We will be adding more in future releases.
 */

/**
 * \defgroup mathlibNode MATHLIB Nodes
 * \brief Node-level APIs supported from MATHLIB.
 */

/** @} */

/**
 * \defgroup SUPERNODE Supernodes
 * \brief SuperNode-level APIs
 */

/**
 * \defgroup errorHandling Error Handling
 * \brief Error-reporting context (ErrorCtxt) used by node and supernode
 *        constructors to report setup failures without exceptions.
 */
