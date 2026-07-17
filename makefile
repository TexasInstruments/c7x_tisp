# ##############################################################################
# TISP top-level Makefile
# Orchestrates xlib builds then TISP for am62d and/or am275.
#
# Uses CMake presets throughout — same build args as:
#   west build-tisp --soc am62d --platform target
#   cmake -S . -B build --preset release-buildlib-am62d-target
#
# Usage:
#   make                             # all xlibs + TISP, am62d+am275, release
#   make SOCS=am62d                  # single SOC
#   make PROFILE=debug               # debug build
#   make xlibs                       # xlibs only (skip TISP)
#   make tisp                        # TISP only  (assumes xlibs already built)
#   make fftlib.am62d                # one xlib, one SOC
#   make audiolib.am275              # audiolib for am275 (builds dsplib first)
#   make clean                       # clean build dirs (keep lib/)
#   make scrub                       # full wipe including lib/
#   make help                        # show this message
# ##############################################################################

SHELL       := /bin/bash
.SHELLFLAGS := -eu -o pipefail -c

# ---------------------------------------------------------------------------
# Knobs — override on the command line
# ---------------------------------------------------------------------------
SOCS    ?= am62d am275
PROFILE ?= release

# ---------------------------------------------------------------------------
# Paths — derived from location of this Makefile so it works from any cwd
# ---------------------------------------------------------------------------
TISP_DIR  := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
WORKSPACE := $(abspath $(TISP_DIR)/..)

FFTLIB_DIR   := $(WORKSPACE)/xlib/fftlib
MATHLIB_DIR  := $(WORKSPACE)/xlib/mathlib
DSPLIB_DIR   := $(WORKSPACE)/xlib/dsplib
AUDIOLIB_DIR := $(WORKSPACE)/xlib/audiolib

# ---------------------------------------------------------------------------
# xlib makefile name, keyed by SOC
# ---------------------------------------------------------------------------
xlib_mk.am62d := makefile.am62dx.c75x.ti-c7000
xlib_mk.am275 := makefile.am275x.c75x.ti-c7000

# ---------------------------------------------------------------------------
# Top-level aggregates
# ---------------------------------------------------------------------------
.PHONY: all xlibs tisp clean scrub help

all: xlibs tisp

xlibs: $(foreach soc,$(SOCS), fftlib.$(soc) mathlib.$(soc) dsplib.$(soc) audiolib.$(soc))

tisp:  $(foreach soc,$(SOCS), tisp.$(soc))

clean: $(foreach soc,$(SOCS), clean.$(soc))

scrub: $(foreach soc,$(SOCS), scrub.$(soc))

# ---------------------------------------------------------------------------
# Per-SOC rules, generated for both am62d and am275.
# $$(MAKE) preserves $(MAKE) in the recipe so jobserver flags are forwarded.
# $(1), $(FFTLIB_DIR), $(PROFILE) etc. are expanded at define-time.
# ---------------------------------------------------------------------------
define soc_rules

.PHONY: fftlib.$(1) mathlib.$(1) dsplib.$(1) audiolib.$(1) \
        tisp.$(1) clean.$(1) scrub.$(1)

fftlib.$(1):
	@echo ">>> fftlib   [$(1)/$(PROFILE)/target]"
	$$(MAKE) -C $(FFTLIB_DIR) -f $(xlib_mk.$(1)) PROFILE=$(PROFILE) all

mathlib.$(1):
	@echo ">>> mathlib  [$(1)/$(PROFILE)/target]"
	$$(MAKE) -C $(MATHLIB_DIR) -f $(xlib_mk.$(1)) PROFILE=$(PROFILE) all

dsplib.$(1):
	@echo ">>> dsplib   [$(1)/$(PROFILE)/target]"
	$$(MAKE) -C $(DSPLIB_DIR) -f $(xlib_mk.$(1)) PROFILE=$(PROFILE) all

# audiolib links against dsplib — always build dsplib first
audiolib.$(1): dsplib.$(1)
	@echo ">>> audiolib [$(1)/$(PROFILE)/target]"
	$$(MAKE) -C $(AUDIOLIB_DIR) -f $(xlib_mk.$(1)) PROFILE=$(PROFILE) all

# TISP: --preset is the single source of truth for all build flags
# Mirrors the convention used by  west build-tisp  and direct cmake usage.
tisp.$(1):
	@echo ">>> tisp     [preset=$(PROFILE)-buildlib-$(1)-target]"
	cmake -S $(TISP_DIR) \
	      -B $(TISP_DIR)/build/$(1)/target/$(PROFILE) \
	      --preset $(PROFILE)-buildlib-$(1)-target \
	      --fresh
	cmake --build $(TISP_DIR)/build/$(1)/target/$(PROFILE) -j

clean.$(1):
	$$(MAKE) -C $(FFTLIB_DIR)   -f $(xlib_mk.$(1)) clean
	$$(MAKE) -C $(MATHLIB_DIR)  -f $(xlib_mk.$(1)) clean
	$$(MAKE) -C $(DSPLIB_DIR)   -f $(xlib_mk.$(1)) clean
	$$(MAKE) -C $(AUDIOLIB_DIR) -f $(xlib_mk.$(1)) clean
	rm -rf $(TISP_DIR)/build/$(1)/

scrub.$(1):
	$$(MAKE) -C $(FFTLIB_DIR)   -f $(xlib_mk.$(1)) scrub
	$$(MAKE) -C $(MATHLIB_DIR)  -f $(xlib_mk.$(1)) scrub
	$$(MAKE) -C $(DSPLIB_DIR)   -f $(xlib_mk.$(1)) scrub
	$$(MAKE) -C $(AUDIOLIB_DIR) -f $(xlib_mk.$(1)) scrub
	rm -rf $(TISP_DIR)/build/$(1)/

endef

$(foreach soc,am62d am275, $(eval $(call soc_rules,$(soc))))

# ---------------------------------------------------------------------------
# Help
# ---------------------------------------------------------------------------
help:
	@echo ""
	@echo "TISP top-level Makefile  —  xlibs + TISP via CMake presets"
	@echo "Same build args as: west build-tisp / cmake --preset <name>"
	@echo ""
	@echo "  make [SOCS='am62d am275'] [PROFILE=release|debug]"
	@echo ""
	@printf "  %-22s %s\n" "all"            "xlibs + TISP for all SOCS  (default)"
	@printf "  %-22s %s\n" "xlibs"          "all four xlibs for all SOCS"
	@printf "  %-22s %s\n" "tisp"           "TISP for all SOCS  (xlibs must exist)"
	@printf "  %-22s %s\n" "fftlib.{soc}"   "fftlib for one SOC"
	@printf "  %-22s %s\n" "mathlib.{soc}"  "mathlib for one SOC"
	@printf "  %-22s %s\n" "dsplib.{soc}"   "dsplib for one SOC"
	@printf "  %-22s %s\n" "audiolib.{soc}" "audiolib for one SOC  (dsplib built first)"
	@printf "  %-22s %s\n" "tisp.{soc}"     "TISP for one SOC"
	@printf "  %-22s %s\n" "clean[.{soc}]"  "remove build dirs, keep lib/"
	@printf "  %-22s %s\n" "scrub[.{soc}]"  "remove build dirs and lib/"
	@echo ""
	@echo "  Defaults:  SOCS='$(SOCS)'  PROFILE=$(PROFILE)"
	@echo "  Workspace: $(WORKSPACE)"
	@echo ""
