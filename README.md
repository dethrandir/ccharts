# ccharts

Terminal charts for financial OHLC data, written as a single-header C library
with optional Python bindings. Line and candlestick charts are rendered as
ANSI-colored strings of Unicode block characters and printed straight to a
terminal.

Licensed under the [MIT License](LICENSE).

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

To build a wheel:

```sh
python3 -m pip wheel . --no-deps -w /tmp/ccharts_wheel
```

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

## Repository layout

- `ccharts.h` — the whole C library (single header, heavily documented).
- `main.c` — C demo using `prices.txt`.
- `prices.txt` — sample JSON OHLC data (THYAO).
- `ccharts/` — Python package: `__init__.py` (high-level `Chart`) and
  `wrapper.c` (CPython extension `ccharts._core`).
- `tests/` — Python test suite (`tests/test_python_api.py`).