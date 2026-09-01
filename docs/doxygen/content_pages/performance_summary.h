// Copyright (C) 2026 Texas Instruments Incorporated
//
// SPDX-License-Identifier: LicenseRef-TI-TFL

// clang-format off

/**
 \page performance_summary Performance Summary

Performance data was obtained on EVM targets with the C7x DSP clocked at 1 GHz.
The approximated equation for computing FFT FLOPs is also shown below.

\f[
\text{FFT FLOPs} = \frac{26}{4} \cdot N \cdot \log_4(N)
\f]

Please note that on AM62A, performance of 2D FFT is 17% less than expected (we are
investigating the degradation). Also note that the DDR throughput on AM62A is
3200 MT/S, while on AM62D it is 3733 MT/S.

\section perf_am62d AM62D (C7504)

\htmlinclude performance_summary_c7504.html

\section perf_am275 AM275 (C7524)

\htmlinclude performance_summary_c7524.html

*/

// clang-format on
