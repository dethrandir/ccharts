# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

`ccharts` is a single-header C library (`ccharts.h`) that renders financial OHLC
data as ANSI-colored terminal charts, plus a thin CPython extension that exposes
it to Python. The C header is the only place chart logic lives;
`ccharts/wrapper.c` and `ccharts/__init__.py` only marshal arguments and manage
memory, and `ccharts/_pandas.py` only reshapes DataFrames into columns.

## Commands

```sh
make test          # compiles main.c (gcc -Wall -g -lm) AND runs the demo on prices.txt
make test-py       # setup.py build_ext --inplace, then the unittest suite
make clean         # rm build/, ccharts/*.so, __pycache__

python3 -m unittest discover -s tests -v                                      # all Python tests
python3 -m unittest tests.test_python_api.TestTimezones -v                    # one class
python3 -m unittest tests.test_python_api.TestCCharts.test_candle_uses_wick   # one test

python3 -m pip install -e ".[pandas]"              # editable install (what CI uses)
python3 -m pip wheel . --no-deps -w /tmp/cc_wheel  # wheel build smoke test
```

The Python targets need `setuptools` installed for the interpreter in use (the
system `python3` here does not have it — use a venv).

CI mirrors these and additionally enforces a **warning-free** compile; keep it
that way:

```sh
gcc -Wall -c ccharts/wrapper.c $(python3-config --includes) -o /tmp/wrapper.o
gcc -Wall main.c -lm -o /tmp/ccharts_demo
```

## Architecture

**Layering.** `ccharts.h` → `ccharts/wrapper.c` (module `ccharts._core`, four
functions: `parse_json`, `parse_arrays`, `create_line`, `create_candle`) →
`ccharts/__init__.py` (class `Chart` with `.line()` / `.candle()`). Behavior
changes belong in the header; the other two layers should stay mechanical.
`main.c` is a C demo that exercises the same path against `prices.txt`.

**Two input paths, one capsule.** `parse_json` (a JSON document) and
`parse_arrays` (four float64 columns + optional int64 epoch seconds) both
produce the same `"ccharts.ohlc"` PyCapsule, so everything downstream is
shared. `parse_arrays` reads each column through the buffer protocol when it
is a 1-D C-contiguous native block and falls back to `PySequence_Fast`
otherwise — that is what makes `Chart.from_arrays` accept lists, numpy arrays
and `array.array` alike. `Chart.from_dataframe` is a thin pandas adapter on
top of it (`ccharts/_pandas.py`), so any regression test for the columnar
path should live in `tests/test_python_api.py` (no pandas needed) rather than
`tests/test_pandas_api.py`.

**NaN and inf must never reach the renderer.** `cc_pixel` feeds values to
`lround()`, which is undefined for non-finite input. `check_finite` in
`wrapper.c` rejects them (`ValueError`); `_pandas.py` drops those rows before
that (`dropna=True`, the default). The check lives in `wrapper.c`, not
`ccharts.h`, so the header keeps its C89-compatible surface.

**Single-header discipline.** `CCHARTS_IMPLEMENTATION` must be defined in exactly
one translation unit — currently `main.c` and `wrapper.c`, which are separate
builds. Everything under that `#ifdef` is private; declarations, the color/block
macros and `CC_MAX_DIM`/`CC_MAX_CELLS` sit above it so clients can use them.

**Render pipeline** (both chart types, in `ccharts.h`):

1. Parse into `cc_ohlc_t[]` — `cc_str_to_ohlc` (CSV) or `cc_json_to_ohlc`
   (fixed-schema JSON, iterative mini-scanner that matches keys exactly).
2. Map data to output columns. When `width < size`, columns downsample: the line
   chart averages the closes in a column, the candle chart aggregates via
   `cc_agg_ohlc` (first open, max high, min low, last close). When
   `width >= size`, candles get several columns each with the last one left as a
   gap.
3. Value → pixel row via `cc_pixel`. **Sub-cell resolution differs per chart:**
   line charts are 8 pixels per cell (`cc_lower_eighth`, `▁▂▃▄▅▆▇█`), candles are
   2 pixels per cell (`▀▄█`) with body/wick tracked as bitmasks (bit0 = lower
   half, bit1 = upper half) and rendered by `cc_render_cell`; body wins over wick.
4. Every cell becomes a **fixed 32-byte string slot** in the `columns` buffer,
   indexed `columns[(x * height + y) * 32]`, with `y` counted from the bottom.
   Any new cell content (color + glyph + `CC_COLOR_RESET`) must fit in 32 bytes
   including the NUL — this budget is what keeps `cc_assemble_chart`'s allocation
   math correct.
5. `cc_assemble_chart` joins rows top-to-bottom, optionally prepending an 8-column
   price margin (max on top row, min on bottom) and appending a timestamp footer
   whose `strftime` format `cc_time_format` picks from the average candle interval.

**Settings.** `cc_settings_t` is meant to be brace-initialized partially;
`cc_settings_resolve` fills unset fields (green rise / red fall, no bg, no area
fill). Never read a settings field without going through it.

**pandas is optional and lazily imported.** `ccharts/_pandas.py` is imported
inside `Chart.from_dataframe`, never at module level — the wheel must install
and work with zero runtime dependencies. The extra is declared as
`[project.optional-dependencies] pandas` and installed only in the first CI
job; `tests/test_pandas_api.py` skips wholesale via `HAS_PANDAS`, which is why
the cibuildwheel wheel tests stay pandas-free.

**Dimension limits are duplicated in three places and must stay in sync:**
`cc_dim_ok` (ccharts.h), `check_dimensions` (wrapper.c), `_check_dimensions`
(`__init__.py`). C returns an empty string for bad dimensions; Python raises
`ValueError` (and `TypeError` for non-int / bool dimensions). The point is that
no input path can trigger a giant or overflowing allocation.

**Timestamps.** `cc_parse_ts` accepts ISO8601 or plain epoch seconds; ISO
detection requires an exact `YYYY-MM-DD` prefix so negative epoch numbers aren't
misread. `cc_iso8601_to_epoch` applies the UTC offset (`+HH:MM`, `+HHMM`, `+HH`,
`Z`) using Hinnant's civil-days algorithm, so the stored epoch is the true
instant. Malformed input parses to `0`, which suppresses the footer.

## Constraints when editing

- **Portable C, no VLAs.** The header must build under MSVC: all scratch buffers
  are heap-allocated and freed on every path (including the allocation-failure
  path), `CC_INLINE` gives functions internal linkage, and `CC_GMTIME_R` wraps
  `gmtime_r`/`gmtime_s`. Don't introduce VLAs, C99-only constructs, or
  compiler-specific extensions.
- **Block characters are `\uXXXX` UCNs**, not literal UTF-8. `setup.py` passes
  `/utf-8` on MSVC so they survive; keep new glyphs in the same UCN form.
- Adding a C source file means updating both `setup.py` (extension sources) and
  `MANIFEST.in` (sdist contents) — `MANIFEST.in` does not affect wheels.

## Releasing

Bump `version` in `pyproject.toml`, then push a tag `v<version>`.
`.github/workflows/publish.yml` fails the release if the tag and the pyproject
version disagree, then builds the sdist plus cibuildwheel wheels (CPython
3.9–3.14; Linux x86_64, macOS x86_64/arm64, Windows AMD64) and uploads to PyPI
with the `PYPI_API_TOKEN` secret. Linux aarch64 is intentionally not built (QEMU
emulation on GitHub runners is too slow).
