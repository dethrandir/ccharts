"""Build script for the ccharts Python package.

The compiled extension is `ccharts._core` (see ccharts/wrapper.c), which
wraps the single-header C library ccharts.h. Build in place with:

    python3 setup.py build_ext --inplace

or build a wheel with pip (the build backend is declared in pyproject.toml):

    python3 -m pip wheel . --no-deps -w /tmp/ccharts_wheel
"""

from setuptools import setup, Extension

ccharts_ext = Extension(
    "ccharts._core",
    sources=["ccharts/wrapper.c"],
)

setup(
    name="ccharts",
    version="0.1.0",
    description="Terminal charts for financial OHLC data (line and candlestick)",
    long_description=open("README.md", encoding="utf-8").read(),
    long_description_content_type="text/markdown",
    license="MIT",
    url="https://github.com/dethrandir/ccharts",
    packages=["ccharts"],
    ext_modules=[ccharts_ext],
    python_requires=">=3.7",
)