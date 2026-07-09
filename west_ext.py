"""west_ext.py — TISP West extension commands.

Adds:
  west build-tisp   build xlib dependencies, MCU+ SDK libs, then TISP

Build philosophy
----------------
xlibs are built explicitly by this script before invoking TISP cmake.
TISP cmake is a pure consumer — it expects pre-built xlib artifacts at
workspace/mcu_plus_sdk/source/{name}/lib/{build_type}/ and uses if(EXISTS) to locate them.

dsplib and audiolib are built sequentially (dsplib first, then audiolib)
directly via cmake presets — no subprocess delegation to audiolib's west command.

MCU+ SDK libraries are built via make-based system with locking to prevent
parallel build conflicts for the same SOC/device (release/debug serialization).

Parallelisation
---------------
Outer — all (soc, platform, build_type) combos via ThreadPoolExecutor.
Inner — per combo:

  fftlib  ──────────────────────┐
  mathlib ──────────────────────┼──► TISP
  dsplib → audiolib ────────────┘   (sequential within one track)
  mcu_sdk ──────────────────────┘   (serialized per SOC/device to prevent conflicts)

subprocess.run() releases the GIL; all threads run in true parallel.

Terminal output
---------------
Set NERD_FONTS=1 to enable nerd font icons.
Colours are auto-disabled when stdout is not a TTY.
"""

import argparse
import os
import shutil
import subprocess
import sys
import threading
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
from textwrap import dedent

try:
    import psutil
    _HAVE_PSUTIL = True
except ImportError:
    _HAVE_PSUTIL = False

# Estimated peak RAM consumed by one (soc × platform × build-type) combo:
# 4 parallel tracks (fftlib, mathlib, dsplib→audiolib, mcu_sdk) × ~500 MB per
# compiler process peak ≈ 2 GB.  Conservative — adjust if your build is leaner.
_GB_PER_COMBO = 2.0

from west.commands import WestCommand

# SOC to device mapping for XLIB artifact naming (used by fftlib, mathlib, dsplib, audiolib)
_SOC_TO_XLIB_DEVICE = {
    "am62d": "C7504",
    "am275": "C7524",
}

# SOC to device mapping for MCU SDK make builds
_SOC_TO_MCU_DEVICE = {
    "am62d": "am62dx",
    "am275": "am275x",
}

# ── MCU SDK build locking ────────────────────────────────────────────────
# Locks to prevent parallel make-based builds for the same SOC/device
# which can cause object file conflicts
_mcu_sdk_locks = {}
_mcu_sdk_lock = threading.Lock()  # Protects access to _mcu_sdk_locks

# Header gen runs once after xlibs are built (xlib artifacts required for cmake configure)
_headers_lock = threading.Lock()
_headers_done = False


def _detect_paths(topdir):
    """Detect workspace layout and return (tisp_dir, xlib_dir, mcu_sdk_dir).

    Supports two workspace layouts:

    Native tisp workspace (workspace-tisp):
      {topdir}/tisp/
      {topdir}/mcu_plus_sdk/source/
      {topdir}/mcu_plus_sdk/

    tvm_byoc workspace (workspace-tvm-byoc):
      {topdir}/packages/mcu_plus_sdk/source/tisp/
      {topdir}/packages/mcu_plus_sdk/source/
      {topdir}/packages/mcu_plus_sdk/
    """
    top = Path(topdir)

    # Native layout: tisp/ at workspace root is the definitive marker.
    # _detect_paths() returns the expected mcu_plus_sdk/ path even if it does
    # not yet exist (e.g., before `west update`).  do_run() validates existence
    # and emits a clear error so the user knows to run `west update`.
    native_tisp = top / "tisp"
    native_sdk = top / "mcu_plus_sdk"
    if native_tisp.is_dir():
        return native_tisp, native_sdk / "source", native_sdk

    # tvm_byoc layout: tisp lives inside packages/mcu_plus_sdk/source/
    pkg_sdk = top / "packages" / "mcu_plus_sdk"
    pkg_tisp = pkg_sdk / "source" / "tisp"
    if pkg_tisp.is_dir():
        return pkg_tisp, pkg_sdk / "source", pkg_sdk

    # Neither found — return native paths so do_run produces the right error message
    return native_tisp, native_sdk / "source", native_sdk

# ── terminal aesthetics ───────────────────────────────────────────────────────

_TTY = sys.stdout.isatty()


def _detect_nerd():
    """Auto-detect nerd font availability.

    Priority:
      1. NERD_FONTS=1/0  — explicit user override always wins
      2. Terminal env vars — kitty, wezterm, ghostty, windows terminal
      3. fc-list          — checks if a nerd font is installed on the system
    """
    val = os.environ.get("NERD_FONTS", "").lower()
    if val in ("1", "true"):
        return True
    if val in ("0", "false"):
        return False

    # Known nerd-font-friendly terminals
    if os.environ.get("KITTY_WINDOW_ID"):
        return True
    if os.environ.get("TERM_PROGRAM") == "WezTerm":
        return True
    if os.environ.get("WT_SESSION"):
        return True  # Windows Terminal
    if os.environ.get("TERM") in ("xterm-kitty", "xterm-ghostty"):
        return True

    # fc-list: nerd fonts installed on this system?
    try:
        out = subprocess.run(
            ["fc-list", "--format=%{family}\n"],
            capture_output=True,
            text=True,
            timeout=2,
        ).stdout
        if any("nerd" in ln.lower() for ln in out.splitlines()):
            return True
    except (FileNotFoundError, subprocess.TimeoutExpired):
        pass

    return False


_NERD = _detect_nerd()


def _c(code):
    return f"\033[{code}m" if _TTY else ""


C_RESET = _c(0)
C_BOLD = _c(1)
C_DIM = _c(2)
C_GREEN = _c(32)
C_RED = _c(31)
C_CYAN = _c(36)
C_YELLOW = _c(33)

I_BUILD = "\uf0ad " if _NERD else "⟳ "  # nf-fa-wrench  / spinner
I_DONE = "\uf00c " if _NERD else "✓ "  # nf-fa-check
I_SKIP = "\uf05e " if _NERD else "⊘ "  # nf-fa-ban
I_FAIL = "\uf00d " if _NERD else "✗ "  # nf-fa-times
I_PKG = "\uf487 " if _NERD else "◈ "  # nf-oct-package


class _Printer:
    """Thread-safe status printer with icons, colours, and elapsed times."""

    def __init__(self):
        self._lock = threading.Lock()
        self._timers = {}

    def _ts(self, label):
        return time.monotonic() - self._timers.get(label, time.monotonic())

    def building(self, label):
        with self._lock:
            self._timers[label] = time.monotonic()
            print(f"  {C_CYAN}{I_BUILD}{C_RESET}{label}", flush=True)

    def done(self, label):
        with self._lock:
            print(
                f"  {C_GREEN}{I_DONE}{C_RESET}{label}  "
                f"{C_DIM}({self._ts(label):.1f}s){C_RESET}",
                flush=True,
            )

    def skipped(self, label):
        with self._lock:
            print(f"  {C_DIM}{I_SKIP}{label} — cached{C_RESET}", flush=True)

    def failed(self, label, log=None):
        with self._lock:
            log_hint = f"  {C_DIM}→ {log}{C_RESET}" if log else ""
            print(
                f"  {C_RED}{I_FAIL}{C_RESET}{label}  "
                f"{C_RED}FAILED{C_RESET}{log_hint}",
                flush=True,
            )

    def print(self, msg):
        """Thread-safe raw print (avoids direct _lock access from outside)."""
        with self._lock:
            print(msg, flush=True)

    def section(self, title):
        with self._lock:
            bar = "─" * (len(title) + 4)
            print(f"\n  ┌{bar}┐")
            print(f"  │  {C_BOLD}{title}{C_RESET}  │")
            print(f"  └{bar}┘", flush=True)


def _summary(title, combos, failed_labels, total_elapsed):
    """Print a summary table of all built combos."""
    n_ok = len(combos) - len(failed_labels)
    n_total = len(combos)

    ok_str = f"{C_GREEN}{I_DONE.strip()}{C_RESET}"
    fail_str = f"{C_RED}{I_FAIL.strip()}{C_RESET}"
    overall = ok_str if not failed_labels else fail_str

    c_lib = len(title)
    c_soc = max(len(s) for s, _, _ in combos)
    c_plat = max(len(p) for _, p, _ in combos)
    c_type = max(len(t) for _, _, t in combos)

    div = (
        f'  ├{"─"*(c_lib+2)}┼{"─"*(c_soc+2)}┼{"─"*(c_plat+2)}┼{"─"*(c_type+2)}┼{"─"*5}┤'
    )
    top = (
        f'  ┌{"─"*(c_lib+2)}┬{"─"*(c_soc+2)}┬{"─"*(c_plat+2)}┬{"─"*(c_type+2)}┬{"─"*5}┐'
    )
    bot = (
        f'  └{"─"*(c_lib+2)}┴{"─"*(c_soc+2)}┴{"─"*(c_plat+2)}┴{"─"*(c_type+2)}┴{"─"*5}┘'
    )

    print(
        f"\n  {C_BOLD}{title}{C_RESET}  {overall}  "
        f"{C_GREEN}{n_ok}{C_RESET}/{n_total} combos  "
        f"{C_DIM}({total_elapsed:.1f}s){C_RESET}"
    )
    print(top)
    print(
        f'  │ {"Library":<{c_lib}} │ {"SOC":<{c_soc}} │ {"Platform":<{c_plat}} │ {"Type":<{c_type}} │  {C_BOLD}  {C_RESET} │'
    )
    print(div)
    for soc, plat, btype in combos:
        combo_label = f"{soc}/{plat}/{btype}"
        is_failed = any(combo_label in f for f in failed_labels)
        icon = (
            f"{C_RED}{I_FAIL.strip()}{C_RESET}"
            if is_failed
            else f"{C_GREEN}{I_DONE.strip()}{C_RESET}"
        )
        print(
            f"  │ {title:<{c_lib}} │ {soc:<{c_soc}} │ {plat:<{c_plat}} │ {btype:<{c_type}} │  {icon}  │"
        )
    print(bot, flush=True)


class BuildTisp(WestCommand):
    """west build-tisp: build xlib dependencies, MCU+ SDK libs, then TISP, all combos in parallel."""

    def __init__(self):
        super().__init__(
            name="build-tisp",
            help="build xlib dependencies then TISP (all combos in parallel)",
            description=dedent("""\
                Parallel per-combo build — all (soc, platform, build-type) combos
                run in separate threads simultaneously.  Within each thread:

                  fftlib  ──────────────────────┐
                  mathlib ──────────────────────┼──► TISP
                  dsplib → audiolib ────────────┘   (sequential within one track)
                  mcu_sdk ──────────────────────┘   (serialized per SOC/device to prevent conflicts)

                Set NERD_FONTS=1 for enhanced terminal icons.
            """),
        )

    def do_add_parser(self, parser_adder):
        parser = parser_adder.add_parser(
            self.name,
            help=self.help,
            description=self.description,
            formatter_class=argparse.RawDescriptionHelpFormatter,
        )
        parser.add_argument(
            "--soc", choices=["am62d", "am275"], nargs="+", default=["am62d", "am275"]
        )
        parser.add_argument(
            "--platform", choices=["pc", "target", "all"], default="all"
        )
        parser.add_argument(
            "--build-type",
            choices=["release", "debug", "all"],
            default="all",
            dest="build_type",
        )
        parser.add_argument(
            "--type",
            choices=["buildlib", "autotest", "clean"],
            default="buildlib",
            dest="preset_type",
            help="buildlib: build library (default); autotest: build+test; "
            "clean: remove cmake build trees and logs for the "
            "matching combos (xlib lib/ artifacts are NOT removed)",
        )
        parser.add_argument("--preset", default=None)
        parser.add_argument("--build-dir", default=None, dest="build_dir")
        parser.add_argument("-j", "--jobs", type=int, default=os.cpu_count())
        parser.add_argument("--fresh", action="store_true", default=False)
        parser.add_argument(
            "--no-throttle",
            action="store_false",
            default=True,
            dest="throttle",
            help="disable make load-average throttling (-l{ncpus}); "
            "faster on powerful machines (e.g. Threadripper), "
            "may OOM on smaller ones",
        )
        parser.add_argument(
            "--mcu-sdk-root",
            default=None,
            dest="mcu_sdk_root",
            help="override auto-detected MCU+ SDK root path "
            "(use for non-standard or future workspace layouts)",
        )
        return parser

    # ── helpers ───────────────────────────────────────────────────────

    def _run_chain(self, *cmds, log=None, env=None, cwd=None):
        """Run commands sequentially, capturing output to log file if given."""

        def _run(fh):
            for cmd in cmds:
                if fh:
                    fh.write(f'$ {" ".join(str(c) for c in cmd)}\n')
                    fh.flush()
                rc = subprocess.run(
                    cmd, stdout=fh, stderr=fh, env=env, cwd=cwd
                ).returncode
                if rc != 0:
                    return rc
            return 0

        if log:
            with open(log, "w") as fh:
                return _run(fh)
        return _run(None)

    def _build_xlib(
        self, name, soc, platform, build_type, jobs, tisp_dir, xlib_dir, env, printer, throttle=True
    ):
        label = f"{I_PKG.strip()} {name:<8}  {soc}/{platform}/{build_type}"
        printer.building(label)
        xlib_src = xlib_dir / name
        xlib_build = tisp_dir / "build" / "xlibs" / soc / platform / build_type / name
        preset = f"{build_type}-buildlib-{soc}-{platform}"
        log = tisp_dir / "build" / "logs" / soc / platform / build_type / f"{name}.log"
        log.parent.mkdir(parents=True, exist_ok=True)
        build_cmd = ["cmake", "--build", str(xlib_build), "--parallel", str(jobs)]
        if throttle:
            build_cmd += ["--", f"-l{jobs}"]
        rc = self._run_chain(
            ["cmake", "-S", str(xlib_src), "-B", str(xlib_build), "--preset", preset],
            build_cmd,
            log=log,
            env=env,
        )
        if rc != 0:
            printer.failed(label, log=log)
            raise RuntimeError(f"{name} build failed for {soc}/{platform}/{build_type}")
        printer.done(label)

    def _build_mcu_sdk(
        self, soc, platform, build_type, jobs, tisp_dir, mcu_sdk_dir, env, printer, throttle=True
    ):
        """Build MCU+ SDK libraries using make-based system.

        This function builds MCU+ SDK components when the mcu_plus_sdk source is
        available in the workspace (typically when using the dev.yml manifest).
        It serializes builds for the same SOC/device to prevent object file
        conflicts between release/debug builds.
        """

        # PC platform: build dmautils host emulation library only.
        # Serialize per-device (same lock key as target) to prevent make races
        # when release/debug combos share the same dmautils source directory.
        if platform == "pc":
            label = f"{I_PKG.strip()} mcu_sdk  {soc}/{platform}/{build_type}"
            if not mcu_sdk_dir.is_dir():
                printer.print(f"  {C_DIM}{I_SKIP}{label} — not present{C_RESET}")
                return
            device = _SOC_TO_MCU_DEVICE[soc]
            dmautils_dir = mcu_sdk_dir / "source" / "drivers" / "dmautils"
            hostemu_mf = f"makefile.{device}.c75x.ti-c7x-hostemu"
            if not (dmautils_dir / hostemu_mf).exists():
                printer.print(
                    f"  {C_DIM}{I_SKIP}{label} — hostemu makefile not found{C_RESET}"
                )
                return
            lock_key = f"{soc}_{device}"
            with _mcu_sdk_lock:
                if lock_key not in _mcu_sdk_locks:
                    _mcu_sdk_locks[lock_key] = threading.Lock()
                lock = _mcu_sdk_locks[lock_key]
            with lock:
                printer.building(label)
                profile = "debug" if build_type == "debug" else "release"
                log_dir = tisp_dir / "build" / "logs" / soc / platform / build_type
                log_dir.mkdir(parents=True, exist_ok=True)
                log = log_dir / "dmautils_hostemu.log"
                tools_path = env.get(
                    "MCU_SDK_TOOLS_PATH", os.path.join(os.path.expanduser("~"), "ti")
                )
                # Generate stub imports.mak if missing (dmautils makefile requires it
                # for CGT_TI_C7X_HOSTEMU_PATH and CGT_TI_C7000_PATH resolution).
                imports_mak = mcu_sdk_dir / "imports.mak"
                if not imports_mak.exists():
                    cgt_root = os.environ.get("CGT7X_ROOT", "")
                    if not cgt_root:
                        cgt_root = os.path.join(tools_path, "ti-cgt-c7000_5.0.0.LTS")
                    imports_mak.write_text(
                        f"# Auto-generated stub for dmautils hostemu build\n"
                        f"export TOOLS_PATH ?= {tools_path}\n"
                        f"CGT_TI_C7000_PATH = {cgt_root}\n"
                        f"CGT_TI_C7X_HOSTEMU_PATH = {cgt_root}\n"
                    )
                make_flags = [f"-j{jobs}"] + ([] if not throttle else [f"-l{jobs}"])
                rc = self._run_chain(
                    ["make", "-f", hostemu_mf, f"PROFILE={profile}"] + make_flags,
                    log=str(log),
                    env={
                        **env,
                        "MCU_PLUS_SDK_PATH": str(mcu_sdk_dir),
                        "TOOLS_PATH": tools_path,
                    },
                    cwd=str(dmautils_dir),
                )
                if rc != 0:
                    printer.failed(label, log=log)
                    raise RuntimeError(
                        f"dmautils hostemu build failed for {soc}/{platform}/{build_type}"
                    )
                printer.done(label)
            return

        device = _SOC_TO_MCU_DEVICE[soc]
        # Create a lock key based on SOC and device (platform doesn't affect MCU SDK build)
        lock_key = f"{soc}_{device}"

        # Get or create lock for this SOC/device combination
        with _mcu_sdk_lock:
            if lock_key not in _mcu_sdk_locks:
                _mcu_sdk_locks[lock_key] = threading.Lock()
            lock = _mcu_sdk_locks[lock_key]

        # Acquire lock for this specific SOC/device to serialize builds
        with lock:
            label = f"{I_PKG.strip()} mcu_sdk  {soc}/{platform}/{build_type}"

            if not mcu_sdk_dir.is_dir():
                printer.print(f"  {C_DIM}{I_SKIP}{label} — not present{C_RESET}")
                return

            printer.building(label)

            profile = "debug" if build_type == "debug" else "release"
            tools_path = env.get(
                "MCU_SDK_TOOLS_PATH", os.path.join(os.path.expanduser("~"), "ti")
            )

            # Generate stub imports.mak if missing (makefiles require it for
            # CGT_TI_C7000_PATH resolution).
            imports_mak = mcu_sdk_dir / "imports.mak"
            if not imports_mak.exists():
                cgt_root = os.environ.get("CGT7X_ROOT", "")
                if not cgt_root:
                    cgt_root = os.path.join(tools_path, "ti-cgt-c7000_5.0.0.LTS")
                imports_mak.write_text(
                    f"# Auto-generated stub for c75x target build\n"
                    f"export TOOLS_PATH ?= {tools_path}\n"
                    f"CGT_TI_C7000_PATH = {cgt_root}\n"
                    f"CGT_TI_C7X_HOSTEMU_PATH = {cgt_root}\n"
                )

            make_flags = [f"-j{jobs}"] + ([] if not throttle else [f"-l{jobs}"])
            build_env = {
                **env,
                "TOOLS_PATH": tools_path,
                "MCU_PLUS_SDK_PATH": str(mcu_sdk_dir),
            }

            log_dir = tisp_dir / "build" / "logs" / soc / platform / build_type
            log_dir.mkdir(parents=True, exist_ok=True)

            # Build MCU+ SDK components needed by TISP target binaries.
            # The full `make libs` fails on sparse checkout (missing submodules),
            # so we build each component individually.
            components = [
                ("freertos", mcu_sdk_dir / "source" / "kernel" / "freertos"),
                ("drivers", mcu_sdk_dir / "source" / "drivers"),
                ("udma", mcu_sdk_dir / "source" / "drivers" / "udma"),
                ("dmautils", mcu_sdk_dir / "source" / "drivers" / "dmautils"),
            ]
            for comp_name, comp_dir in components:
                mf = f"makefile.{device}.c75x.ti-c7000"
                if not (comp_dir / mf).exists():
                    printer.print(
                        f"  {C_DIM}{I_SKIP}{comp_name} target makefile not found{C_RESET}"
                    )
                    continue
                log_file = log_dir / f"{comp_name}_target.log"
                rc = self._run_chain(
                    ["make", "-f", mf, f"PROFILE={profile}"] + make_flags,
                    log=str(log_file),
                    env=build_env,
                    cwd=str(comp_dir),
                )
                if rc != 0:
                    printer.failed(label, log=log_file)
                    raise RuntimeError(
                        f"{comp_name} target build failed for {soc}/{platform}/{build_type}"
                    )
            printer.done(label)

    # ── per-combo worker ──────────────────────────────────────────────

    def _build_combo(self, soc, platform, build_type, args, max_combo_workers, tisp_dir, xlib_dir, mcu_sdk_dir, printer):
        label = f"{soc}/{platform}/{build_type}"

        jobs = args.jobs
        # Calculate dynamic job/worker limits to enforce overall jobs cap
        track_jobs = max(1, jobs // (max_combo_workers * 4))
        inner_workers = min(4, max(1, jobs // max_combo_workers))
        tisp_jobs = max(1, jobs // max_combo_workers)

        env = dict(os.environ)
        env["MCU_PLUS_SDK_ROOT"] = str(mcu_sdk_dir)

        def _audiolib_track():
            self._build_xlib("dsplib", soc, platform, build_type, track_jobs, tisp_dir, xlib_dir, env, printer, args.throttle)
            self._build_xlib("audiolib", soc, platform, build_type, track_jobs, tisp_dir, xlib_dir, env, printer, args.throttle)

        with ThreadPoolExecutor(
            max_workers=inner_workers
        ) as pool:  # dynamically scaled tracks: fftlib, mathlib, audiolib, mcu_sdk
            futures = {
                pool.submit(
                    self._build_xlib,
                    "fftlib",
                    soc,
                    platform,
                    build_type,
                    track_jobs,
                    tisp_dir,
                    xlib_dir,
                    env,
                    printer,
                    args.throttle,
                ): "fftlib",
                pool.submit(
                    self._build_xlib,
                    "mathlib",
                    soc,
                    platform,
                    build_type,
                    track_jobs,
                    tisp_dir,
                    xlib_dir,
                    env,
                    printer,
                    args.throttle,
                ): "mathlib",
                pool.submit(_audiolib_track): "audiolib",
                pool.submit(
                    self._build_mcu_sdk,
                    soc,
                    platform,
                    build_type,
                    track_jobs,
                    tisp_dir,
                    mcu_sdk_dir,
                    env,
                    printer,
                    args.throttle,
                ): "mcu_sdk",
            }
            errors = []
            for f in as_completed(futures):
                if f.exception():
                    errors.append(str(f.exception()))
        if errors:
            raise RuntimeError("\n".join(errors))

        # Generate test case headers once — xlibs must be built first so cmake
        # configure succeeds. First combo to reach here generates; others wait.
        global _headers_done
        if args.preset_type == "autotest":
            with _headers_lock:
                if not _headers_done:
                    hlabel = f"{I_PKG.strip()} gen_headers  (once, shared)"
                    printer.building(hlabel)
                    gen_preset = f"{build_type}-autotest-{soc}-{platform}"
                    gen_dir = tisp_dir / "build" / soc / platform / build_type
                    gen_log = (
                        tisp_dir
                        / "build"
                        / "logs"
                        / soc
                        / platform
                        / build_type
                        / "gen_headers.log"
                    )
                    gen_log.parent.mkdir(parents=True, exist_ok=True)
                    _ncpus = os.cpu_count() or 4
                    _build_cmd = [
                        "cmake",
                        "--build",
                        str(gen_dir),
                        "--target",
                        "gen_all_test_case_headers",
                        "--parallel",
                    ]
                    if args.throttle:
                        _build_cmd += ["--", f"-l{_ncpus}"]
                    rc = self._run_chain(
                        [
                            "cmake",
                            "-S",
                            str(tisp_dir),
                            "-B",
                            str(gen_dir),
                            "--preset",
                            gen_preset,
                        ],
                        _build_cmd,
                        log=str(gen_log),
                        env=env,
                    )
                    if rc != 0:
                        printer.failed(hlabel, log=gen_log)
                        raise RuntimeError(
                            f"gen_all_test_case_headers failed — see {gen_log}"
                        )
                    _headers_done = True
                    printer.done(hlabel)

        tisp_label = f"{I_PKG.strip()} TISP       {label}"
        tisp_preset = args.preset or f"{build_type}-{args.preset_type}-{soc}-{platform}"
        build_dir = (
            Path(args.build_dir) / soc / platform / build_type
            if args.build_dir
            else tisp_dir / "build" / soc / platform / build_type
        )
        log = tisp_dir / "build" / "logs" / soc / platform / build_type / "tisp.log"
        log.parent.mkdir(parents=True, exist_ok=True)
        configure = [
            "cmake",
            "-S",
            str(tisp_dir),
            "-B",
            str(build_dir),
            "--preset",
            tisp_preset,
        ]
        if args.fresh:
            configure.append("--fresh")
        printer.building(tisp_label)
        # Headers already generated once in do_run before parallel builds.
        build_cmd = ["cmake", "--build", str(build_dir), "--parallel", str(tisp_jobs)]
        if args.throttle:
            build_cmd += ["--", f"-l{tisp_jobs}"]
        rc = self._run_chain(configure, build_cmd, log=log)
        if rc != 0:
            printer.failed(tisp_label, log=log)
            raise RuntimeError(f"TISP build failed for {label}")
        printer.done(tisp_label)

    # ── clean ─────────────────────────────────────────────────────────

    def _do_clean(self, combos, tisp_dir, xlib_dir, printer):
        """Remove cmake build trees and logs for the given combos.

        Removes per-combo directories under tisp/build/:
          {soc}/{platform}/{build_type}/          — TISP cmake build
          xlibs/{soc}/{platform}/{build_type}/    — xlib cmake builds
          logs/{soc}/{platform}/{build_type}/     — log files

        xlib lib/ artifacts (<xlib_dir>/*/lib/) are intentionally left alone.
        To force a full xlib rebuild, remove them manually:
          rm -rf <xlib_dir>/*/lib/
        """
        printer.section(f"west build-tisp --type clean  —  {len(combos)} combos")
        cleaned = 0
        for soc, plat, bt in combos:
            label = f"{I_PKG.strip()} clean  {soc}/{plat}/{bt}"
            dirs = [
                tisp_dir / "build" / soc / plat / bt,
                tisp_dir / "build" / "xlibs" / soc / plat / bt,
                tisp_dir / "build" / "logs" / soc / plat / bt,
            ]
            removed = [d for d in dirs if d.exists()]
            if not removed:
                printer.skipped(label)
                continue
            for d in removed:
                shutil.rmtree(d)
            n = len(removed)
            printer.print(
                f"  {C_GREEN}{I_DONE}{C_RESET}{label}  "
                f'{C_DIM}({n} dir{"s" if n > 1 else ""} removed){C_RESET}'
            )
            cleaned += 1

        printer.print(
            f"\n  {C_DIM}Cleaned {cleaned}/{len(combos)} combos.  "
            f"xlib lib/ artifacts untouched — "
            f"rm -rf {xlib_dir}/*/lib/ to force-rebuild from source.{C_RESET}\n"
        )

    # ── main entry point ──────────────────────────────────────────────

    def do_run(self, args, unknown):
        tisp_dir, xlib_dir, mcu_sdk_dir = _detect_paths(self.topdir)
        if args.mcu_sdk_root is not None:
            new_mcu_sdk_dir = Path(args.mcu_sdk_root)
            # In the tvm_byoc layout tisp lives inside mcu_plus_sdk/source/,
            # so recompute tisp_dir when mcu_sdk_dir is overridden.
            # Path.is_relative_to() requires Python 3.9+; use relative_to() for compat.
            try:
                _in_sdk = tisp_dir.is_relative_to(mcu_sdk_dir)
            except AttributeError:  # Python < 3.9
                try:
                    tisp_dir.relative_to(mcu_sdk_dir)
                    _in_sdk = True
                except ValueError:
                    _in_sdk = False
            if _in_sdk:
                tisp_dir = new_mcu_sdk_dir / "source" / "tisp"
            mcu_sdk_dir = new_mcu_sdk_dir
            xlib_dir = mcu_sdk_dir / "source"
        if not tisp_dir.is_dir():
            self.die(f"tisp/ not found: {tisp_dir}")

        platforms = ["pc", "target"] if args.platform == "all" else [args.platform]
        build_types = (
            ["release", "debug"] if args.build_type == "all" else [args.build_type]
        )
        combos = [
            (soc, plat, bt)
            for soc in args.soc
            for plat in platforms
            for bt in build_types
        ]

        printer = _Printer()

        if args.preset_type == "clean":
            self._do_clean(combos, tisp_dir, xlib_dir, printer)
            return

        if not xlib_dir.is_dir():
            self.die(f'mcu_plus_sdk/source/ not found at {xlib_dir} — run "west update" first')
        if not mcu_sdk_dir.is_dir():
            self.die(f'mcu_plus_sdk/ not found at {mcu_sdk_dir} — run "west update" first')

        printer.section(f"west build-tisp  —  {len(combos)} combos")
        print(f"  {C_DIM}workspace : {self.topdir}{C_RESET}")
        print(
            f"  {C_DIM}combos    : "
            + "  ".join(f"{s}/{p}/{bt}" for s, p, bt in combos)
            + C_RESET,
            flush=True,
        )

        t0 = time.monotonic()
        failed = []
        # Cap outer combo parallelism by both CPU count and available RAM.
        # Each combo runs up to 4 parallel cmake/make tracks; peak ~_GB_PER_COMBO GB per combo.
        # Load-average throttling (-l{ncpus}) inside each cmake bounds CPU but not RAM,
        # so memory-aware capping here prevents OOM on machines with <_GB_PER_COMBO GB/combo.
        #
        # Note: available memory is sampled once at build start (static cap).  If other
        # processes consume RAM after the executor launches, the cap will not adapt.
        # This is an intentional trade-off: re-checking per-combo requires a polling loop
        # and is unlikely to matter for typical developer machines.  If your system has
        # heavy background load, set TISP_MAX_WORKERS=N to pin the parallelism manually.
        ncpus = os.cpu_count() or 4
        if _HAVE_PSUTIL:
            avail_gb = psutil.virtual_memory().available / (1024 ** 3)
            mem_workers = max(1, int(avail_gb / _GB_PER_COMBO))
        else:
            mem_workers = ncpus  # fall back to cpu-only limit
        env_workers = os.environ.get("TISP_MAX_WORKERS")
        if env_workers is not None:
            try:
                max_workers = max(1, int(env_workers))
            except ValueError:
                self.die(f"TISP_MAX_WORKERS must be a positive integer, got: {env_workers!r}")
        else:
            max_workers = min(len(combos), ncpus, mem_workers)
        with ThreadPoolExecutor(max_workers=max_workers) as executor:
            futures = {
                executor.submit(
                    self._build_combo, soc, plat, bt, args, max_workers, tisp_dir, xlib_dir, mcu_sdk_dir, printer
                ): f"{soc}/{plat}/{bt}"
                for soc, plat, bt in combos
            }
            for future in as_completed(futures):
                if future.exception():
                    failed.append(f"{futures[future]}: {future.exception()}")

        _summary("TISP", combos, failed, time.monotonic() - t0)

        if failed:
            self.die("\nFailed combos:\n  " + "\n  ".join(failed))
