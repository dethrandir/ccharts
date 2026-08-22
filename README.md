# ccharts

Terminal charts for financial OHLC data, written as a single-header C library
with optional Python bindings. Line and candlestick charts are rendered as
ANSI-colored strings of Unicode block characters and printed straight to a
terminal.

Licensed under the [MIT License](LICENSE).

[![PyPI version](https://img.shields.io/pypi/v/ccharts.svg)](https://pypi.org/project/ccharts)
[![License](https://img.shields.io/pypi/l/ccharts.svg)](https://pypi.org/project/ccharts)
*Badges become live once the first release is published.*

## Installation

```sh
pip install ccharts
```

For pandas DataFrame support (optional — the package itself has no runtime
dependencies):

```sh
pip install ccharts[pandas]
```

Pre-built wheels for Linux (x86_64 and aarch64), macOS (x86_64 and arm64)
and Windows (x86_64), built on GitHub Actions CI for CPython 3.9-3.14, are
published together with each release — no C toolchain is needed on the
target machine. Install the sdist instead and a C compiler (gcc/Clang on
Linux/macOS, MSVC on Windows) is required, because the package wraps the C
single-header library `ccharts.h`.

## Features

- **Line charts** — smooth curves using 8-level vertical resolution
  (`▁▂▃▄▅▆▇█`), per-segment coloring, optional area fill under the line.
- **Candlestick charts** — solid bodies between open/close, thin vertical
  wicks (`│`) between high/low, gap-separated candles, automatic
  downsampling when there are more candles than columns.
- **Axis labels** — optional max/min price margin and first/last timestamp
  footer. The timestamp format is picked automatically from the candle
  interval (`YYYY-MM-DD` for daily+, `HH:MM` for intraday).
- **Timestamps** — optional per-candle epoch timestamps, parsed from ISO8601
  or a plain number.
- **Parsers** — CSV (`cc_str_to_ohlc`) and a fixed-schema JSON parser
  (`cc_json_to_ohlc`).
- **pandas / columnar input** (Python) — `Chart.from_dataframe(df)` and
  `Chart.from_arrays(...)` copy price columns straight into the C array, with
  no JSON round-trip. pandas is optional.

## C usage

```c
#define CCHARTS_IMPLEMENTATION
#include "ccharts.h"

int main(void) {
    const char* json = ...;                 // [{ts,open,high,low,close}, ...]
    cc_ohlc_t* ohlc = NULL;
    int size = 0;
    cc_json_to_ohlc(json, &ohlc, &size);

    cc_settings_t s = { .rise_color = CC_COLOR_BLUE,
                        .show_prices = 1, .show_times = 1 };
    char* chart = cc_line_create(ohlc, size, 60, 8, &s);
    printf("%s\n", chart);

    free(chart);
    free(ohlc);
    return 0;
}
```

Compile with `-lm` (math lib). `CCHARTS_IMPLEMENTATION` must be defined in
exactly one translation unit; every other unit can include the header as-is.
C++ code can use the header for declarations (see the doc comment in
ccharts.h for details).

Run the demo with `make test` — it compiles `main.c` **and runs the binary**,
rendering `prices.txt` as a line chart and two candlestick variants.

## Python usage

```sh
make test-py        # builds the extension and runs the test suite
```

```python
from ccharts import Chart

chart = Chart(open("prices.txt").read())
print(chart.line(width=60, height=8, show_prices=True, show_times=True))
print(chart.candle(width=60, height=8))
```

### DataFrames and raw columns

A DataFrame does not have to be serialized to JSON first — the price columns
are handed to C directly:

```python
import yfinance as yf
from ccharts import Chart

df = yf.download("THYAO.IS", period="3mo")     # Open/High/Low/Close columns
print(Chart.from_dataframe(df).candle(width=80, height=12, show_times=True))
```

- Columns are matched case-insensitively (`Open`/`open`/`OPEN`); pass
  `ohlc=("o", "h", "l", "c")` for anything else.
- Timestamps come from a `DatetimeIndex`, or from `ts="column"`. Timezone-aware
  values are converted to UTC, naive ones are treated as UTC.
- Rows with NaN/inf in an OHLC column are dropped (indicator frames have NaN
  warm-up rows); `dropna=False` raises instead.
- Row order is used as-is — a descending frame renders right to left.

The same path works without pandas, for lists, numpy arrays or `array.array`:

```python
Chart.from_arrays(opens, highs, lows, closes, ts=epoch_seconds)
```

To develop locally (builds the extension in place):

```sh
python3 -m pip install -e .      # or: make test-py
```

Building a wheel by hand also works and is a good local smoke test:

```sh
python3 -m pip wheel . --no-deps -w /tmp/ccharts_wheel
```

## C ABI and other languages

`ccharts.h` exports nothing — `CC_INLINE` makes every function `static inline`,
and `struct cc_ohlc` is private to the implementation block — so there is
nothing for a foreign function interface to bind to. `abi/ccharts_abi.h`
provides that layer: a flat, `extern "C"`, opaque-handle API with explicit
status codes, built as a shared or static library.

```sh
cmake -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

```c
#include "ccharts_abi.h"

ccharts_data* data = NULL;
ccharts_from_arrays(open, high, low, close, ts, n, &data);

ccharts_settings s = {0};
s.rise_color = ccharts_color(CCHARTS_COLOR_BLUE);
s.show_prices = 1;

char* chart = NULL;
size_t len = 0;
if (ccharts_line(data, 60, 8, &s, &chart, &len) == CCHARTS_OK) {
    printf("%s", chart);
    ccharts_string_free(chart);
}
ccharts_data_free(data);
```

Differences from the raw header, all in the direction of being bindable:
invalid dimensions return `CCHARTS_ERR_DIMENSIONS` instead of an empty string,
NaN and inf are rejected up front, and `ccharts_parse_csv` counts the rows it
can parse so a dataset never carries trailing zero-filled candles.

`conformance/cases.json` plus `conformance/golden/*.txt` are the cross-language
contract: 20 cases covering both chart types, downsampling, labels, timezones,
flat ranges and single-candle input. The goldens are generated from the ABI
(`scripts/gen_golden.py`) and every binding — the Python one included, see
`tests/test_conformance.py` — must reproduce them byte for byte.

### Language bindings

| Language | Location | Status |
| -------- | -------- | ------ |
| Python   | `ccharts/` | published on PyPI |
| Rust     | `bindings/rust/` | builds and passes conformance; not published yet |
| Go       | `bindings/go/` | builds and passes conformance; not published yet |
| JavaScript (WASM) | `bindings/js/` | builds and passes conformance; not published yet |
| C# (.NET 8) | `bindings/dotnet/` | builds and passes conformance; not published yet |
| Java (JDK 22+) | `bindings/java/` | builds and passes conformance; not published yet |

Rust and Go compile the vendored C sources with their own toolchains (the `cc`
crate and cgo), so neither needs a prebuilt library:

```rust
let chart = Chart::from_arrays(&open, &high, &low, &close, Some(&ts))?;
println!("{}", chart.line(60, 8, &Settings::new().rise(Color::Blue))?);
```

```go
chart, _ := ccharts.FromArrays(open, high, low, close, ts)
defer chart.Close()
out, _ := chart.Candle(60, 8, &ccharts.Options{ShowPrices: true})
```

The JavaScript package is the library compiled to WebAssembly, with the module
embedded in the JavaScript — one dependency-free package for Node, Deno, Bun
and browsers alike, no prebuilt binaries and no bundler configuration:

```js
import { Chart, Color } from "ccharts";
const chart = Chart.fromArrays(open, high, low, close, ts);
console.log(chart.candle({ width: 60, height: 8, showPrices: true }));
chart.free();
```

C# uses source-generated P/Invoke and Java the Foreign Function & Memory API
(no JNI code at all). Both link the shared library rather than compiling the C,
so both ship prebuilt natives — `runtimes/{rid}/native/` in the NuGet package,
`native/{os}-{arch}/` in the jar:

```csharp
using var chart = Chart.FromArrays(open, high, low, close, ts);
Console.Write(chart.Line(new ChartOptions { RiseColor = Color.Blue, ShowPrices = true }));
```

```java
try (Chart chart = Chart.fromArrays(open, high, low, close, ts)) {
    System.out.print(chart.line(ChartOptions.builder().rise(Color.BLUE).build()));
}
```

Each binding vendors its own copy of `ccharts.h` and the ABI — Go needs cgo
sources inside the package directory and `cargo package` only ships crate-local
files — so `scripts/sync_sources.py` refreshes them and CI fails if a copy
drifts.

## Settings

| Field         | Meaning                                                    | Default         |
| ------------- | ---------------------------------------------------------- | --------------- |
| `rise_color`  | color for rising values / candles                          | green           |
| `fall_color`  | color for falling values / candles                         | red             |
| `bg_color`    | background of empty cells                                  | none            |
| `area_color`  | fill below the line chart                                  | none            |
| `single_color`| 1 = one line color; 0 = per-segment colors                  | 0               |
| `show_prices` | print max/min price labels                                 | 0               |
| `show_times`  | print first/last timestamp footer                          | 0               |

Any unset field falls back to its default, so only the fields you want to
override need to be set (see `cc_settings_resolve`).

## Input constraints

- **Timestamps** — ISO8601 strings may carry a UTC offset (`+03:00`, `+0300`,
  `+03`, `Z`; a missing offset is treated as UTC). The offset is applied, so
  the stored epoch is the true instant: `2026-07-20T00:00:00+03:00` becomes
  `2026-07-19T21:00:00` UTC. Plain epoch-seconds numbers (including
  negative) are accepted as-is. Malformed timestamps parse to `0`.
- **Dimensions** — `width`/`height` must be positive and are capped at
  `CC_MAX_DIM` (100000) per side and `CC_MAX_CELLS` (1000000) total cells.
  Invalid dimensions make the C chart functions return an empty string and
  make the Python bindings raise `ValueError` — no unchecked giant
  allocation, no stack overflow.
- **CSV** — lines may be arbitrarily long and at most `size` lines are read.
- **JSON** — must be an array of objects with `ts`, `open`, `high`, `low`,
  `close` (plus optional ignored fields like `volume`). The parser is an
  iterative mini-scanner: keys are matched exactly, so a substring like
  `"open"` inside a string value can never cause a false match. Malformed or
  empty documents make the parsers fail cleanly.

## Windows / MSVC

The header is VLA-free and uses only portable C89 constructs, so it compiles
with MSVC out of the box: `gmtime_r` is swapped for `gmtime_s` on `_WIN32`
and all functions get internal linkage (`static`) through the `CC_INLINE`
macro, sidestepping MSVC's lack of C11 inline semantics. No extra flags or
libraries are required beyond the standard ones.

## Releases

Releases are published automatically from GitHub Actions: pushing a tag
`v<version>` (e.g. `v0.2.0`) runs `.github/workflows/publish.yml`, which
verifies that the tag matches the version in `pyproject.toml`, builds the
sdist and all platform wheels with cibuildwheel, and uploads them to PyPI.

Publishing currently authenticates with the `PYPI_API_TOKEN` repository
secret (an API token scoped to the `ccharts` project). An alternative is
[trusted publishing](https://docs.pypi.org/trusted-publishers/), which
removes the token entirely: link the `pypi` GitHub environment to PyPI,
then delete the `password` input from the publish step in the workflow.

## Repository layout

- `ccharts.h` — the whole C library (single header, heavily documented).
- `main.c` — C demo using `prices.txt`.
- `prices.txt` — sample JSON OHLC data (THYAO).
- `abi/` — flat C ABI (`ccharts_abi.h`/`.c`) for non-Python bindings.
- `bindings/` — language bindings built on that ABI (`rust/`, `go/`, `js/`,
  `dotnet/`, `java/`).
- `conformance/` — cross-language case list, goldens and a C smoke test.
- `scripts/` — golden generation, vendored-source sync, version consistency.
- `ccharts/` — Python package: `__init__.py` (high-level `Chart`),
  `wrapper.c` (CPython extension `ccharts._core`) and `_pandas.py` (optional
  DataFrame conversion, imported lazily).
- `tests/` — Python test suite (`test_python_api.py`, plus `test_pandas_api.py`
  which skips when pandas is not installed).