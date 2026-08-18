"""Build script for the ccharts Python package.

The compiled extension is `ccharts._core` (see ccharts/wrapper.c), which
wraps the single-header C library ccharts.h. Build in place with:

    python3 setup.py build_ext --inplace
"""

from setuptools import setup, Extension

ccharts_ext = Extension(
    "ccharts._core",
    sources=["ccharts/wrapper.c"],
)

setup(
    name="ccharts",
    version="0.1.0",
    packages=["ccharts"],
    ext_modules=[ccharts_ext]
)
