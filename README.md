# ccharts

Terminal charts for financial OHLC data, written as a single-header C library
with optional Python bindings. Line and candlestick charts are rendered as
ANSI-colored strings of Unicode block characters and printed straight to a
terminal.

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

Run the demo with `make test` (reads `prices.txt`).

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

## Repository layout

- `ccharts.h` — the whole C library (single header, heavily documented).
- `main.c` — C demo using `prices.txt`.
- `prices.txt` — sample JSON OHLC data (THYAO).
- `ccharts/` — Python package: `__init__.py` (high-level `Chart`) and
  `wrapper.c` (CPython extension `ccharts._core`).
- `tests/` — Python test suite (`python3 -m unittest tests.test_python_api`).
- `setup.py`, `pyproject.toml` — packaging for the Python bindings.
