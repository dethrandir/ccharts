"""Build script for the ccharts Python extension `ccharts._core`.

All package metadata (name, version, description, ...) lives in
pyproject.toml (PEP 621); this file only declares the compiled extension
that wraps the single-header C library ccharts.h.

The C compiler is picked up automatically by setuptools (gcc on
Linux/macOS, MSVC on Windows). On MSVC the chart characters are embedded
as \\uXXXX universal character names; without /utf-8 they would be
converted to the system ANSI codepage and unrepresentable glyphs (U+2502
wick, the 1/8 block bars) would degrade to '?' at runtime.
"""

import sys

from setuptools import setup, Extension

extra_compile_args = ["/utf-8"] if sys.platform == "win32" else []

setup(
    ext_modules=[
        Extension(
            "ccharts._core",
            sources=["ccharts/wrapper.c"],
            extra_compile_args=extra_compile_args,
        ),
    ],
)