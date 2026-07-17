// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

// clang-format off

#if defined(__C7504__)
/**
 \page performance_summary Performance Summary

Performance data was obtained on the AM62D EVM. C7x clocked at 1 GHz.
Approximated equation for computing FFT FLOPs is also shown below.

FFT FLOPs = 26/4 * N * log4(N)

Please note that on AM62A, performance of 2D FFT is 17% less than expected (we are investigating the degradation).
Also, note that the DDR throughput on AM62A is 3200 MT/S, while on AM62D, it is 3733 MT/S.


\htmlinclude ./html/performance_summary_c7504.html

*/



#endif
// clang-format on
