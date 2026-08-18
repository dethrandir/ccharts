"""Build script for the ccharts Python extension `ccharts._core`.

All package metadata (name, version, description, ...) lives in
pyproject.toml (PEP 621); this file only declares the compiled extension
that wraps the single-header C library ccharts.h.

The C compiler is picked up automatically by setuptools (gcc on
Linux/macOS, MSVC on Windows); no extra flags are needed.
"""

from setuptools import setup, Extension

setup(
    ext_modules=[
        Extension(
            "ccharts._core",
            sources=["ccharts/wrapper.c"],
        ),
    ],
)