"""Generate TISP_delayDru_idat.h, TISP_delayDru_druDelay_idat.cpp,
and TISP_delayDru_druDelay_memory_map.hpp from test_cases_list.csv.

CSV columns:
  testID       - integer test identifier
  numChannels  - number of audio channels for this case
  frameSamples - samples per channel per frame
  maxDelay     - max(delaySamples[0..numChannels-1]) — ring sizing
  delaySamples - bracket-array of per-channel delays (always 8 values; extra values
                 for channels beyond numChannels are 0 and unused by the kernel)
  numFrames    - number of frames to run (for on-target profiling depth)
  numReps      - warm-loop repetition count

Buffer sizing (computed per case, max taken for pool):
  ringBytesPerCh   = next_pow2(maxDelay + frameSamples) * sizeof(float)
  ringBytesTotal   = numChannels * ringBytesPerCh
  stateBytesTotal  = numChannels * (maxDelay + frameSamples) * sizeof(float)
  frameElems       = numChannels * frameSamples

DelayDru alignment requirement:
  gRingDdr must be aligned to ringBytesPerCh (so the DRU circular mask
  wraps correctly within each channel's block). The generated MAX_RING_BYTES_PER_CH
  is used as the alignment attribute in the memory_map.
"""

from __future__ import print_function
import csv
import os
import math

MAX_CHANNELS = 8   # fixed array length in the C struct


def parse_array(s):
    """Parse '[a,b,c]' -> list of ints"""
    return [int(x.strip()) for x in s.strip().strip("[]").split(",") if x.strip()]


def next_pow2(v):
    """Return smallest power of two >= v."""
    if v <= 1:
        return 1
    return 1 << math.ceil(math.log2(v))


def load_test_cases(csv_path):
    cases = []
    with open(csv_path, encoding="utf-8-sig") as f:
        reader = csv.DictReader(f)
        for row in reader:
            # Strip whitespace from keys and values (CSV may have spaces after commas)
            row = {k.strip(): v.strip() for k, v in row.items() if k is not None}
            test_id      = int(row["testID"])
            num_ch       = int(row["numChannels"])
            frame_smp    = int(row["frameSamples"])
            max_delay    = int(row["maxDelay"])
            delays       = parse_array(row["delaySamples"])
            num_frames   = int(row["numFrames"])
            num_reps     = int(row["numReps"])

            assert num_ch <= MAX_CHANNELS, \
                f"testID {test_id}: numChannels={num_ch} exceeds MAX_CHANNELS={MAX_CHANNELS}"
            assert len(delays) == MAX_CHANNELS, \
                f"testID {test_id}: delaySamples must have exactly {MAX_CHANNELS} entries (pad extras with 0)"
            assert max(delays[:num_ch]) == max_delay, \
                f"testID {test_id}: maxDelay={max_delay} does not match max(delaySamples[:numChannels])={max(delays[:num_ch])}"
            assert all(d == 0 for d in delays[num_ch:]), \
                f"testID {test_id}: delaySamples entries beyond numChannels must be 0"

            cases.append({
                "testID":       test_id,
                "numChannels":  num_ch,
                "frameSamples": frame_smp,
                "maxDelay":     max_delay,
                "delaySamples": delays,
                "numFrames":    num_frames,
                "numReps":      num_reps,
            })
    return cases


def compute_pool_params(cases):
    """Compute max buffer sizes across all test cases."""
    max_ring_bytes_per_ch = 0
    max_ring_bytes_total  = 0
    max_frame_elems       = 0

    B_THRESHOLD = 700 * 1024  # cases with stateBytesTotal <= this run Approach B

    # Compute max state only for B-applicable cases (L2/DDR state buffers are B-only)
    max_state_bytes_for_b = 0

    for c in cases:
        ring_bytes_per_ch = next_pow2(c["maxDelay"] + c["frameSamples"]) * 4
        ring_bytes_total  = c["numChannels"] * ring_bytes_per_ch
        state_bytes_total = c["numChannels"] * (c["maxDelay"] + c["frameSamples"]) * 4
        frame_elems       = c["numChannels"] * c["frameSamples"]

        max_ring_bytes_per_ch = max(max_ring_bytes_per_ch, ring_bytes_per_ch)
        max_ring_bytes_total  = max(max_ring_bytes_total,  ring_bytes_total)
        max_frame_elems       = max(max_frame_elems,       frame_elems)

        if state_bytes_total <= B_THRESHOLD:
            max_state_bytes_for_b = max(max_state_bytes_for_b, state_bytes_total)

    max_channels      = max(c["numChannels"] for c in cases)
    max_state_elems   = max_state_bytes_for_b // 4
    max_ring_samples  = max_ring_bytes_total  // 4  # total ring as float elements (for gRefRing)

    return {
        "max_channels":        max_channels,
        "max_ring_bytes_per_ch": max_ring_bytes_per_ch,
        "max_ring_bytes_total":  max_ring_bytes_total,
        "max_state_bytes_total": max_state_bytes_for_b,
        "max_state_elems":       max_state_elems,
        "max_frame_elems":       max_frame_elems,
        "max_ring_samples":      max_ring_samples,
    }


def write_idat_h(path, cases, pool):
    max_ch = pool["max_channels"]
    with open(path, "w") as f:
        f.write("/* This file is autogenerated by gen_data.py — do not edit */\n")
        f.write("#pragma once\n\n")
        f.write("#include <stdint.h>\n\n")
        f.write(f"#define TISP_DDT_MAX_CHANNELS {max_ch}U\n\n")
        f.write("#ifdef __cplusplus\n")
        f.write('extern "C" {\n')
        f.write("#endif\n\n")
        f.write("typedef struct {\n")
        f.write("   uint32_t testID;\n")
        f.write("   uint32_t numChannels;\n")
        f.write("   uint32_t frameSamples;\n")
        f.write("   uint32_t maxDelay;          /* max across all channels — ring sizing */\n")
        f.write("   uint32_t delaySamples[TISP_DDT_MAX_CHANNELS]; /* per-channel delay in samples */\n")
        f.write("   uint32_t numFrames;\n")
        f.write("   uint32_t numReps;           /* warm-loop repetition count */\n")
        f.write("} TISP_delayDru_testParams_t;\n\n")
        f.write("void TISP_delayDru_getTestParams(\n")
        f.write("        TISP_delayDru_testParams_t **params,\n")
        f.write("        int32_t                                *numTests);\n\n")
        f.write("#ifdef __cplusplus\n")
        f.write("}\n")
        f.write("#endif\n")


def write_idat_cpp(path, cases):
    with open(path, "w") as f:
        f.write("/* This file is autogenerated by gen_data.py — do not edit */\n")
        f.write('#include "TISP_delayDru_idat.h"\n\n')
        f.write("static TISP_delayDru_testParams_t testParams[] = {\n")
        for c in cases:
            delays_str = ", ".join(str(d) for d in c["delaySamples"])
            f.write("   {\n")
            f.write(f"      {c['testID']},                    /* testID */\n")
            f.write(f"      {c['numChannels']}U,              /* numChannels */\n")
            f.write(f"      {c['frameSamples']}U,             /* frameSamples */\n")
            f.write(f"      {c['maxDelay']}U,                 /* maxDelay */\n")
            f.write(f"      {{ {delays_str} }},  /* delaySamples */\n")
            f.write(f"      {c['numFrames']}U,               /* numFrames */\n")
            f.write(f"      {c['numReps']}U,                 /* numReps */\n")
            f.write("   },\n")
        f.write("};\n\n")
        f.write("void TISP_delayDru_getTestParams(\n")
        f.write("        TISP_delayDru_testParams_t **params,\n")
        f.write("        int32_t                                *numTests)\n")
        f.write("{\n")
        f.write("   *params   = testParams;\n")
        f.write("   *numTests = (int32_t)(sizeof(testParams) /\n")
        f.write("                        sizeof(TISP_delayDru_testParams_t));\n")
        f.write("}\n")


def write_memory_map(path, pool):
    mrbpc = pool["max_ring_bytes_per_ch"]   # ring alignment
    mrbt  = pool["max_ring_bytes_total"]    # gRingDdr / gRefRing size
    msbt  = pool["max_state_bytes_total"]   # gDelayState* size in bytes
    mse   = pool["max_state_elems"]         # gDelayState* size in floats
    mfe   = pool["max_frame_elems"]         # gInFrame / gOutFrame / gScratch size in floats
    mrs   = pool["max_ring_samples"]        # gRefRing size in floats

    with open(path, "w") as f:
        f.write("/* This file is autogenerated by gen_data.py — do not edit */\n")
        f.write("#pragma once\n\n")
        f.write("#include <stdint.h>\n\n")
        f.write("/* ── Sizing macros (max across all test cases) ──────────────────────────── */\n")
        f.write(f"/* Ring buffer: per-channel size is next_pow2(maxDelay+frameSamples)*4        */\n")
        f.write(f"/* Ring alignment: pRingDdr[c*ch_bytes] must be ch_bytes-aligned for DRU     */\n")
        f.write(f"#define TISP_DDT_MAX_RING_BYTES_PER_CH  {mrbpc}U\n")
        f.write(f"#define TISP_DDT_MAX_RING_BYTES_TOTAL   {mrbt}U\n")
        f.write(f"#define TISP_DDT_MAX_RING_SAMPLES_TOTAL {mrs}U  /* = TOTAL/sizeof(float) */\n")
        f.write(f"#define TISP_DDT_MAX_STATE_BYTES_TOTAL  {msbt}U\n")
        f.write(f"#define TISP_DDT_MAX_STATE_ELEMS        {mse}U\n")
        f.write(f"#define TISP_DDT_MAX_FRAME_ELEMS        {mfe}U\n\n")

        f.write("#if defined(HOST_EMULATION)\n\n")
        f.write(f"static float   gInFrameL2    [TISP_DDT_MAX_FRAME_ELEMS];\n")
        f.write(f"static float   gOutFrameL2_A [TISP_DDT_MAX_FRAME_ELEMS];\n")
        f.write(f"static float   gOutFrameL2_B [TISP_DDT_MAX_FRAME_ELEMS];\n")
        f.write(f"static float   gScratchL2    [TISP_DDT_MAX_FRAME_ELEMS];\n")
        f.write(f"static float   gDelayStateL2 [TISP_DDT_MAX_STATE_ELEMS];\n")
        f.write(f"static uint8_t gRingDdr      [TISP_DDT_MAX_RING_BYTES_TOTAL];\n")
        f.write(f"static float   gDelayStateDdr[TISP_DDT_MAX_STATE_ELEMS];\n")
        f.write(f"static float   gRefRing      [TISP_DDT_MAX_RING_SAMPLES_TOTAL];\n\n")
        f.write("#else\n\n")
        f.write("/* Target: L2SRAM-resident working buffers. */\n")
        f.write('__attribute__((section(".l2sramData"), aligned(64)))\n')
        f.write(f"static float gInFrameL2[TISP_DDT_MAX_FRAME_ELEMS];\n")
        f.write('__attribute__((section(".l2sramData"), aligned(64)))\n')
        f.write(f"static float gOutFrameL2_A[TISP_DDT_MAX_FRAME_ELEMS];\n")
        f.write('__attribute__((section(".l2sramData"), aligned(64)))\n')
        f.write(f"static float gOutFrameL2_B[TISP_DDT_MAX_FRAME_ELEMS];\n")
        f.write('__attribute__((section(".l2sramData"), aligned(64)))\n')
        f.write(f"static float gScratchL2[TISP_DDT_MAX_FRAME_ELEMS];\n")
        f.write('__attribute__((section(".l2sramData"), aligned(64)))\n')
        f.write(f"static float gDelayStateL2[TISP_DDT_MAX_STATE_ELEMS];\n\n")
        f.write("/* DDR: ring must be aligned to ringBytesPerCh for DRU circular masking.\n")
        f.write(" * DEVIATION: ideally non-cacheable DDR (.ddr_state_nc/MAIR4); using .ddrData\n")
        f.write(" * as a pragmatic placeholder (correctness unaffected on HOST_EMULATION). */\n")
        f.write('__attribute__((section(".ddrData"),\n')
        f.write(f"              aligned(TISP_DDT_MAX_RING_BYTES_PER_CH)))\n")
        f.write(f"static uint8_t gRingDdr[TISP_DDT_MAX_RING_BYTES_TOTAL];\n\n")
        f.write('__attribute__((section(".ddrData"), aligned(128)))\n')
        f.write(f"static float gDelayStateDdr[TISP_DDT_MAX_STATE_ELEMS];\n\n")
        f.write("/* Reference ring (CPU model) — same layout as gRingDdr but as float* */\n")
        f.write('__attribute__((section(".ddrData"),\n')
        f.write(f"              aligned(TISP_DDT_MAX_RING_BYTES_PER_CH)))\n")
        f.write(f"static float gRefRing[TISP_DDT_MAX_RING_SAMPLES_TOTAL];\n\n")
        f.write("#endif\n")


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    csv_path   = os.path.join(script_dir, "test_cases_list.csv")
    out_dir    = os.path.join(script_dir, "../../../misc/TISP_delayDru")

    cases = load_test_cases(csv_path)
    pool  = compute_pool_params(cases)

    print(f"  test cases              : {len(cases)}")
    print(f"  TISP_DDT_MAX_CHANNELS   : {pool['max_channels']}")
    print(f"  MAX_RING_BYTES_PER_CH   : {pool['max_ring_bytes_per_ch']}")
    print(f"  MAX_RING_BYTES_TOTAL    : {pool['max_ring_bytes_total']}")
    print(f"  MAX_STATE_BYTES_TOTAL   : {pool['max_state_bytes_total']}")
    print(f"  MAX_STATE_ELEMS         : {pool['max_state_elems']}")
    print(f"  MAX_FRAME_ELEMS         : {pool['max_frame_elems']}")
    print(f"  MAX_RING_SAMPLES_TOTAL  : {pool['max_ring_samples']}")

    # The idat .h/.cpp files are regenerated every build. The memory_map.hpp
    # is hand-maintained (adds the AM275 OCRAM/HyperRAM variants + 4 MB cap
    # that this generator cannot express), so it is NOT overwritten here.
    write_idat_h(
        os.path.join(out_dir, "TISP_delayDru_idat.h"),
        cases, pool)
    write_idat_cpp(
        os.path.join(out_dir, "TISP_delayDru_idat.cpp"),
        cases)
    print("Done.")


if __name__ == "__main__":
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("-a", "--allCases", action="store_true",
                    help="generate all cases (default)")
    ap.parse_args()   # accept -a flag passed by gen_all_test_case_headers.sh
    main()
