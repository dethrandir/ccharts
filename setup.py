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
