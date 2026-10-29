# Configuration file for the Sphinx documentation builder.
#
# This file is part of the TISP documentation build.
#
# Theme and CSS override approach (html_theme + theme_overrides.css) are
# deliberately mirrored from TexasInstruments/processor-sdk-doc:
#   html_theme = 'sphinx_rtd_theme'
#   html_logo  pointing at _static/img/ti_logo.svg
#   html_show_sphinx = False
#   def setup(app): app.add_css_file("css/theme_overrides.css")
#
# All DEVFAMILY / OS / tag-loader machinery present in processor-sdk-doc has
# been omitted — TISP is a C++ middleware library and does not need it.
#
# Doxygen XML is produced by the dedicated doxyfile_sphinx (not by Exhale).
# Pass the output directory via the TISP_DOXYGEN_XML environment variable:
#   export TISP_DOXYGEN_XML=/path/to/build/docs/xml
#   sphinx-build -b html docs/sphinx /path/to/output

import os
import sys

# ---------------------------------------------------------------------------
# Project information
# ---------------------------------------------------------------------------

project   = "TISP"
author    = "Texas Instruments Incorporated"
copyright = "2026, Texas Instruments Incorporated"

# Show the source branch under the logo so the rendered docs correlate with
# the code they were generated from. Priority: explicit env override
# (TISP_DOC_REF, set by CI) -> current git branch -> "main".
import subprocess


def _doc_ref():
    ref = os.environ.get("TISP_DOC_REF")
    if ref:
        return ref.strip()
    try:
        out = subprocess.run(
            ["git", "rev-parse", "--abbrev-ref", "HEAD"],
            cwd=os.path.dirname(os.path.abspath(__file__)),
            capture_output=True, text=True, timeout=5)
        branch = out.stdout.strip()
        if branch and branch != "HEAD":
            return branch
    except Exception:
        pass
    return "main"


version = _doc_ref()
release = version

# ---------------------------------------------------------------------------
# Doxygen XML directory
# ---------------------------------------------------------------------------

_fallback_xml_dir = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", "..", "build", "docs", "xml")
)

xml_dir = os.environ.get("TISP_DOXYGEN_XML", _fallback_xml_dir)

if not os.path.isdir(xml_dir):
    import warnings
    warnings.warn(
        f"[conf.py] Doxygen XML directory not found: {xml_dir!r}\n"
        "  Set TISP_DOXYGEN_XML to the path produced by running doxygen on "
        "docs/doxygen/doxyfile_sphinx, or run Doxygen first to generate the "
        "XML before invoking sphinx-build.",
        stacklevel=2,
    )

# ---------------------------------------------------------------------------
# Extensions
# ---------------------------------------------------------------------------

extensions = [
    "breathe",
    "sphinx.ext.mathjax",
    "sphinx_design",
    "sphinx_copybutton",
    "sphinx.ext.graphviz",
    "sphinx_immaterial",
]

# ---------------------------------------------------------------------------
# Source / master document
# ---------------------------------------------------------------------------

master_doc = "index"

# ---------------------------------------------------------------------------
# C++ language settings (TISP is C++, not C)
# ---------------------------------------------------------------------------

primary_domain     = "cpp"
highlight_language = "cpp"

pygments_style = "friendly"
# No maximum_signature_line_length: let signatures render single-line in the
# gray sig block (natural wrap) — the clean look. Forcing multi-line adds a
# per-line break + trailing comma that reads clumsily for long constructors.

# ---------------------------------------------------------------------------
# Breathe configuration
# ---------------------------------------------------------------------------

breathe_projects        = {"tisp": xml_dir}
breathe_default_project = "tisp"
# TISP is C++.  Parse .hpp/.cpp as the cpp domain.
breathe_domain_by_extension = {"hpp": "cpp", "cpp": "cpp"}

# ---------------------------------------------------------------------------
# API reference generation (per-group Doxygen groups)
# ---------------------------------------------------------------------------
# TISP organises every node class as a member of one of seven Doxygen groups
# (\defgroup / \addtogroup / \ingroup in each node header).  The node
# description — including LaTeX formulas embedded in the header comments —
# lives in the group's detailed description, which Exhale's file/function
# tree does not surface.  Instead we emit one page per group using Breathe's
# ``.. doxygengroup::`` directive (which renders the overview prose + formulas
# + all member classes / functions).  This auto-scales to every new node that
# is tagged \ingroup <groupName>.

import glob
import xml.etree.ElementTree as ET

_API_DIR = os.path.join(os.path.dirname(__file__), "api")

# Groups rendered without ``:members:`` because their header has an unbalanced
# Doxygen group (``@{`` with no ``@}``), which duplicates a member under ``-W``.
# Remove entries here once the corresponding header is fixed.
_NO_MEMBERS_GROUPS = set()

# Ordered API grouping for the index page.  Each entry is
# (category_title, [group_stem, ...]) matching the README node taxonomy.
# Any group discovered in the XML but NOT listed here is emitted under a
# final "Other" toctree and triggers a build warning.
_API_GROUP_ORDER = [
    ("Framework", [
        "NODE",
        "SUPERNODE",
    ]),
    ("Error Handling", [
        "errorHandling",
    ]),
    ("Audio Nodes", [
        "audiolibNode",
    ]),
    ("DSP Nodes", [
        "dsplibNode",
    ]),
    ("FFT Nodes", [
        "fftlibNode",
    ]),
    ("Math Nodes", [
        "mathlibNode",
    ]),
    ("Utility Nodes", [
        "miscNode",
    ]),
]


# Node-library groups that get per-class pages (one .rst per innerclass).
# Framework groups (NODE, SUPERNODE, errorHandling) remain as single group pages.
_NODE_LIB_GROUPS = {"audiolibNode", "dsplibNode", "fftlibNode", "mathlibNode", "miscNode"}


def _short_name(fqn):
    """Return the last :: segment of a fully-qualified class name.

    Examples:
      TISP::AUDIOLIB::Balance  -> Balance
      addConstant              -> addConstant
    """
    return fqn.split("::")[-1].strip()


def _parse_group(path):
    """Return (group_name, group_title, [(fqn, short_name), ...]) for one XML file."""
    try:
        root = ET.parse(path).getroot()
    except ET.ParseError:
        return None
    compound = root.find("./compounddef[@kind='group']")
    if compound is None:
        return None
    name_el = compound.find("compoundname")
    if name_el is None or not name_el.text:
        return None
    name = name_el.text.strip()
    title_el = compound.find("title")
    title = (title_el.text.strip() if title_el is not None and title_el.text else name)
    classes = []
    for ic in compound.findall("innerclass"):
        fqn = (ic.text or "").strip()
        if fqn:
            classes.append((fqn, _short_name(fqn)))
    return name, title, classes


def _caption_to_slug(caption):
    """Convert a caption like 'Audio Nodes' to a slug like 'audio_nodes'."""
    return caption.lower().replace(" ", "_")


# Captions whose stems are all node-lib groups (get a landing page + per-class
# sub-pages).  Any caption NOT in this set is treated as a framework category
# (its stems already have their own single-group pages; the caption itself
# becomes a landing page only when it has more than one stem; a single-stem
# non-node-lib category is listed directly in api/index).
_FRAMEWORK_SINGLE_STEMS = {"errorHandling"}  # listed directly in api/index

# Single group pages whose classes live in their own namespace.
_GROUP_NAMESPACES = {"SUPERNODE": "TISP::SuperNode"}


def _generate_api_pages(xml_directory, api_directory):
    """Emit api/index.rst + per-page RST files for all Doxygen groups.

    Layout produced:
    - Per-class pages for node-lib groups: ``api/<ShortName>.rst``
    - Landing pages for node-lib captions: ``api/<slug>.rst``
      (e.g., ``api/audio_nodes.rst``) with a toctree of that group's class pages.
    - Landing page for "Framework": ``api/framework.rst`` listing NODE + SUPERNODE.
    - ``api/errorHandling.rst`` as a standalone single-group page (listed
      directly in api/index since it has only one stem and no sub-classes).
    - ``api/index.rst``: ONE toctree (:maxdepth: 2, no :caption:) listing all
      landing pages in _API_GROUP_ORDER order so sphinx-immaterial renders each
      caption as a real nav section.

    For node-library groups (``_NODE_LIB_GROUPS``), one page is generated per
    inner class using ``.. doxygenclass::``.  If a node-lib group has no
    innerclasses it falls back to the single ``.. doxygengroup::`` page.

    For framework groups (NODE, SUPERNODE) the existing single
    ``.. doxygengroup::`` page is retained — these are small and carry overview
    prose that belongs together.
    """
    if not os.path.isdir(xml_directory):
        return
    os.makedirs(api_directory, exist_ok=True)

    # Parse every group XML.
    parsed_groups = {}  # stem -> (title, [(fqn, short)])
    for path in sorted(glob.glob(os.path.join(xml_directory, "group__*.xml"))):
        result = _parse_group(path)
        if result is None:
            continue
        name, title, classes = result
        parsed_groups[name] = (title, classes)

    # Track which RST page names are emitted so we can build the index.
    # Maps stem -> list of toctree entries (relative, no .rst extension).
    group_pages = {}  # stem -> [page_name, ...]

    for name, (title, classes) in parsed_groups.items():
        if name in _NODE_LIB_GROUPS and classes:
            # Per-class pages.
            page_names = []
            for fqn, short in sorted(classes, key=lambda x: x[1].lower()):
                page_file = os.path.join(api_directory, f"{short}.rst")
                underline = "=" * max(len(short), 3)
                with open(page_file, "w", encoding="utf-8") as fh:
                    fh.write(f"{short}\n{underline}\n\n")
                    fh.write(f".. doxygenclass:: {fqn}\n   :members:\n")
                page_names.append(short)
            group_pages[name] = page_names
        else:
            # Single group page (framework groups, or node-lib fallback).
            underline = "=" * max(len(title), 3)
            page_file = os.path.join(api_directory, f"{name}.rst")
            with open(page_file, "w", encoding="utf-8") as fh:
                fh.write(f"{title}\n{underline}\n\n")
                opts = "" if name in _NO_MEMBERS_GROUPS else "\n   :members:"
                if name in _GROUP_NAMESPACES:
                    # Declare the namespace so C++ xrefs to it (e.g. the base
                    # class TISP::SuperNode::BaseSuperNode) resolve.
                    fh.write(f".. doxygennamespace:: {_GROUP_NAMESPACES[name]}\n"
                             "   :desc-only:\n\n")
                fh.write(f".. doxygengroup:: {name}{opts}\n")
            group_pages[name] = [name]

    # Detect ungrouped stems.
    grouped_stems = {stem for _cat, stems in _API_GROUP_ORDER for stem in stems}
    discovered = set(parsed_groups.keys())
    ungrouped = sorted(discovered - grouped_stems)
    if ungrouped:
        print(f"WARNING: ungrouped API page(s): {', '.join(ungrouped)}")

    # -----------------------------------------------------------------------
    # Build landing pages for each _API_GROUP_ORDER caption.
    # landing_index_entries: ordered list of page stems for api/index toctree.
    # -----------------------------------------------------------------------
    landing_index_entries = []  # page stems to list in api/index toctree

    for category, stems in _API_GROUP_ORDER:
        # Collect pages for all stems in this category.
        all_stems_are_single_direct = (
            len(stems) == 1 and stems[0] in _FRAMEWORK_SINGLE_STEMS
        )

        if all_stems_are_single_direct:
            # errorHandling: list its single page directly in api/index.
            stem = stems[0]
            if stem in group_pages:
                landing_index_entries.extend(group_pages[stem])
            continue

        # All other categories: emit a landing page.
        slug = _caption_to_slug(category)
        landing_page = slug  # e.g. "audio_nodes", "framework"

        # Collect the toctree entries for this landing page.
        toc_entries = []
        for s in stems:
            if s in group_pages:
                toc_entries.extend(group_pages[s])

        if not toc_entries:
            continue

        # Write the landing page.
        underline = "=" * max(len(category), 3)
        landing_file = os.path.join(api_directory, f"{slug}.rst")
        with open(landing_file, "w", encoding="utf-8") as fh:
            fh.write(f"{category}\n{underline}\n\n")
            fh.write(
                f".. toctree::\n"
                f"   :maxdepth: 1\n\n"
            )
            for entry in toc_entries:
                fh.write(f"   {entry}\n")
            fh.write("\n")

        landing_index_entries.append(landing_page)

    # -----------------------------------------------------------------------
    # Collect the full set of RST page names we just generated (for stale cleanup).
    # -----------------------------------------------------------------------
    all_emitted = set()
    # Per-class and single-group pages.
    for pages in group_pages.values():
        all_emitted.update(pages)
    # Landing pages.
    for category, stems in _API_GROUP_ORDER:
        all_stems_are_single_direct = (
            len(stems) == 1 and stems[0] in _FRAMEWORK_SINGLE_STEMS
        )
        if not all_stems_are_single_direct:
            slug = _caption_to_slug(category)
            all_emitted.add(slug)
    all_emitted.add("index")

    # Remove any stale .rst files left from a previous build that used a
    # different generation strategy (e.g. old per-group pages for node libs).
    for stale in glob.glob(os.path.join(api_directory, "*.rst")):
        stem = os.path.splitext(os.path.basename(stale))[0]
        if stem not in all_emitted:
            os.remove(stale)

    # -----------------------------------------------------------------------
    # api/index.rst: single toctree listing all landing pages in order.
    # -----------------------------------------------------------------------
    with open(os.path.join(api_directory, "index.rst"), "w",
              encoding="utf-8") as fh:
        fh.write(":icon: material/book-open-variant\n\n")
        fh.write("API Reference\n=============\n\n")
        fh.write(
            ".. toctree::\n"
            "   :maxdepth: 2\n\n"
        )
        for entry in landing_index_entries:
            fh.write(f"   {entry}\n")
        if ungrouped:
            fh.write("\n")
            for stem in ungrouped:
                for pg in group_pages.get(stem, [stem]):
                    fh.write(f"   {pg}\n")
        fh.write("\n")


_generate_api_pages(xml_dir, _API_DIR)


# ---------------------------------------------------------------------------
# Performance pages
# ---------------------------------------------------------------------------
# Port the two device HTML table fragments directly from
# docs/doxygen/html/performance_summary_c7504.html  (AM62D)
# docs/doxygen/html/performance_summary_c7524.html  (AM275)
# Each is a self-contained <table> fragment; embed via ``.. raw:: html``.

_PERF_DIR      = os.path.join(os.path.dirname(__file__), "perf")
_PERF_HTML_DIR = os.path.join(os.path.dirname(__file__), "..", "doxygen", "html")

# (html_filename_stem, human_title)
_PERF_DEVICES = [
    ("performance_summary_c7504", "C7504 (AM62D)"),
    ("performance_summary_c7524", "C7524 (AM275)"),
]


def _generate_perf_pages(html_dir, perf_dir, devices):
    """Emit perf/index.rst + one perf/<slug>.rst per device table."""
    os.makedirs(perf_dir, exist_ok=True)
    written = []

    for stem, title in devices:
        html_path = os.path.join(html_dir, f"{stem}.html")
        if not os.path.isfile(html_path):
            continue
        slug = stem.replace("performance_summary_", "")
        underline = "=" * len(title)
        with open(os.path.join(perf_dir, f"{slug}.rst"), "w",
                  encoding="utf-8") as fh:
            fh.write(f"{title}\n{underline}\n\n")
            fh.write(".. raw:: html\n\n")
            with open(html_path, encoding="utf-8", errors="ignore") as tbl:
                for line in tbl.read().splitlines():
                    fh.write(f"   {line}\n")
            fh.write("\n")
        written.append((slug, title))

    with open(os.path.join(perf_dir, "index.rst"), "w",
              encoding="utf-8") as fh:
        fh.write(":icon: material/chart-line\n\n")
        fh.write("Performance\n===========\n\n")
        fh.write(
            "Measured supernode performance on EVM targets with the C7x DSP "
            "clocked at 1 GHz.  The approximated equation for computing FFT FLOPs is:\n\n"
            ".. math::\n\n"
            "   \\text{FFT FLOPs} = \\frac{26}{4} \\cdot N \\cdot \\log_4(N)\n\n"
            ".. note::\n\n"
            "   On AM62A, performance of 2D FFT is approximately 17% less than\n"
            "   expected (under investigation).  DDR throughput on AM62A is\n"
            "   3200 MT/S vs 3733 MT/S on AM62D.\n\n"
        )
        fh.write(".. toctree::\n   :maxdepth: 1\n\n")
        for slug, _title in written:
            fh.write(f"   {slug}\n")


_generate_perf_pages(_PERF_HTML_DIR, _PERF_DIR, _PERF_DEVICES)


# ---------------------------------------------------------------------------
# MathJax (version 3 is the Sphinx default; AMS extensions load automatically)
# ---------------------------------------------------------------------------
# TISP node headers use \f$...\f$ and \f[...\f] in Doxygen comments;
# Breathe passes these through as :math:` ` and .. math:: directives,
# which MathJax renders in the browser.

# ---------------------------------------------------------------------------
# HTML output
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# Warning suppression
# ---------------------------------------------------------------------------
# Some Doxygen @code blocks in TISP headers contain Unicode box-drawing art
# (MIXER, MULTIPLEXER diagrams).  Breathe passes them through with language
# "cpp"; Pygments fails on the Unicode box chars and retries in relaxed mode
# successfully -- so the output is correct, only the first-pass attempt
# triggers a warning.  Suppress this specific warning category.
suppress_warnings = ["misc.highlighting_failure"]

html_theme        = "sphinx_immaterial"
html_title        = "TISP Documentation"
html_static_path  = ["_static"]
html_logo         = "_static/img/ti_logo_mono.svg"

html_show_sphinx      = False
html_show_sourcelink  = False
html_copy_source      = False
html_last_updated_fmt = "%b %d, %Y"

html_theme_options = {
    # Palette: calm single light scheme with TI-adjacent neutral tones.
    # No dark-mode toggle; user can tune later.
    "palette": [
        {
            "scheme": "default",
            "primary": "red",
            "accent":  "teal",
        },
    ],
    # Features: product-docs chrome — top tabs, grouped sections, instant
    # loading, code copy buttons.
    "features": [
        "navigation.top",   # back-to-top button
        "search.highlight", # highlight search terms after navigation
        "toc.follow",       # TOC sidebar tracks scroll position
    ],
    # Source repo shown in the header: GitHub logo + repo name.
    # The branch label ("main") is added via the source.html template
    # override (see _templates/partials/source.html).
    "repo_url":  "https://github.com/TexasInstruments/c7x_tisp",
    "repo_name": "c7x_tisp",
    "icon": {"repo": "material/github"},
}

# Template overrides (partials/source.html adds the branch badge).
templates_path = ["_templates"]

# ---------------------------------------------------------------------------
# Theme customisation hook
#
# theme_overrides.css targets .wy-* RTD classes that don't exist in
# sphinx-immaterial — load it anyway (harmless, no matching selectors).
# Add a second CSS file targeting sphinx-immaterial's .md-* selectors for:
#   - WIP amber banner via .md-content__inner::before
#   - C++ signature token colouring (same palette as RTD version, new selectors)
# ---------------------------------------------------------------------------

def setup(app):
    # Original RTD overrides (harmless, no matching selectors in immaterial).
    app.add_css_file("css/theme_overrides.css")
    # Immaterial-specific overrides: WIP banner + sig colouring.
    app.add_css_file("css/immaterial_overrides.css")
