# ccharts

[![Version](https://img.shields.io/badge/version-v3.0.0-blue.svg)](https://github.com/dethrandir/ccharts)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![PyPI version](https://img.shields.io/pypi/v/ccharts.svg)](https://pypi.org/project/ccharts)
[![crates.io](https://img.shields.io/crates/v/ccharts.svg)](https://crates.io/crates/ccharts)
[![npm](https://img.shields.io/npm/v/@dethrandir/ccharts.svg)](https://www.npmjs.com/package/@dethrandir/ccharts)
[![NuGet](https://img.shields.io/nuget/v/Ccharts.svg)](https://www.nuget.org/packages/Ccharts)
[![Maven Central](https://img.shields.io/maven-central/v/io.github.dethrandir/ccharts.svg)](https://central.sonatype.com/artifact/io.github.dethrandir/ccharts)
[![RubyGems](https://img.shields.io/gem/v/ccharts.svg)](https://rubygems.org/gems/ccharts)
[![LuaRocks](https://img.shields.io/luarocks/v/dethrandir/ccharts.svg)](https://luarocks.org/modules/dethrandir/ccharts)

**Financial and statistical data in, a string out.** High-density terminal charts drawn with Unicode block characters and returned as plain text strings — so the chart goes wherever text goes: a terminal, a log line, a chat message, an HTML `<pre>`, a commit comment, or a `printf`.

```text
  328.00████████████                                                
                   █                                                
                   █           █████████████▁▁▁▁▁▁▁▁▁▁▁▁            
                   █████████████                       █            
                                                       █            
                                                       █            
                                                       █            
  301.00                                               █▁▁▁▁▁▁▁▁▁▁▁▁
2026-07-20                                                2026-07-24
```

There is no canvas, no image buffer, no GUI, and no terminal detection hooks — one function call returns a string, and what you do with it is your business.

Written as a single-header C89 library with a flat C ABI and idiomatic bindings for **Python**, **Rust**, **Go**, **JavaScript/WASM**, **.NET/C#**, **Java**, **Ruby**, **Lua**, and **Julia**. Every binding produces byte-identical output across platforms.

---

## Language Ecosystem

| Language | Package / Target | Registry | Installation Command |
| :--- | :--- | :--- | :--- |
| **Python** | `ccharts` | [PyPI](https://pypi.org/project/ccharts) | `pip install ccharts` (or `pip install ccharts[pandas]`) |
| **Rust** | `ccharts` | [crates.io](https://crates.io/crates/ccharts) | `cargo add ccharts` |
| **Go** | `ccharts` | [GitHub](https://github.com/dethrandir/ccharts) | `go get github.com/dethrandir/ccharts/bindings/go/ccharts` |
| **JavaScript / WASM** | `@dethrandir/ccharts` | [npm](https://www.npmjs.com/package/@dethrandir/ccharts) | `npm install @dethrandir/ccharts` |
| **.NET / C#** | `Ccharts` | [NuGet](https://www.nuget.org/packages/Ccharts) | `dotnet add package Ccharts` |
| **Java** (JDK 22+) | `io.github.dethrandir:ccharts` | [Maven Central](https://central.sonatype.com/artifact/io.github.dethrandir/ccharts) | Maven / Gradle dependency |
| **Ruby** | `ccharts` | [RubyGems](https://rubygems.org/gems/ccharts) | `gem install ccharts` |
| **Lua** | `ccharts` | [LuaRocks](https://luarocks.org/modules/dethrandir/ccharts) | `luarocks install ccharts` |
| **Julia** | `Ccharts` | [General](https://github.com/dethrandir/ccharts) | `using Pkg; Pkg.add("Ccharts")` |

---

## Supported Chart Types

`ccharts` supports **9 visual chart types**, each optimized with fractional Unicode glyphs:

```
  1. Line Chart          2. Candlestick          3. Pie / Donut
  328.00██████             330.25▄▄▄▄▄ █████               ████████      
              █                         │                ████████████    
              █                                         ██████  ██████   
              █▁▁▁▁                     █████           ████      ████   
                                300.75  █████             ██████████     
                                2026-07-20              Kira  40 (40%)

  4. Histogram           5. Sparkline            6. Categorical Bar
         8     ████      ▂▃▅▆▇██▇▆▅▄▃            100.00      ████        
               ████                                          ████  ████  
         1 ████████                              0.00  ████  ████  ████  
       3.10    7.90                                    Q1    Q2    Q3    

  7. Stacked Bar         8. Heatmap              9. Box Plot
       150.00  ████      Mon █ █ █ █ █ █         120.00        │         
               ████      Tue █ █ █ █ █ █                       ████      
         0.00  ████      Wed █ █ █ █ █ █          10.00  ▂     █   │     
               Prod      Thu █ █ █ █ █ █                 Cat A Cat B     
```

1. **Line (`line`)**: High-resolution curves using 8 vertical sub-pixel levels (`▁▂▃▄▅▆▇█`). Supports area fills, single or directional colors, and automatic downsampling.
2. **Candlestick (`candle`)**: Open, High, Low, Close (OHLC) financial charts with solid bodies (`▀▄█`), thin vertical wicks (`│`), and automatic downsampling (`cc_agg_ohlc`).
3. **Pie & Donut (`pie`)**: Proportional charts with terminal aspect ratio compensation, custom hole sizes (`donut=True`), slice gaps, center text, and customizable legends.
4. **Histogram (`histogram` / `hist`)**: Frequency distributions across continuous sample data with automatic or custom binning and outlier clamping.
5. **Sparkline (`sparkline` / `spark`)**: Compact, axis-less micro trend lines (height 1 to 3) for inline dashboards and log lines.
6. **Bar (`bar`)**: Categorical bar charts scaled from a shared zero baseline with 8 sub-pixel tops and categorical label footers.
7. **Stacked Bar (`stacked_bar` / `stack`)**: Multi-series part-to-whole categorical breakdowns showing total sums and series segments.
8. **Heatmap (`heatmap` / `heat`)**: 2-D matrix density visualization using a 10-step deterministic colormap ladder and 2-D block-average downsampling.
9. **Box Plot (`boxplot` / `box`)**: Statistical 5-number summaries ($Min, Q_1, Median, Q_3, Max$) computed deterministically via nearest-rank quartiles.

---

## Quickstart Examples

### C (Single-Header)
```c
#define CCHARTS_IMPLEMENTATION
#include "ccharts.h"

int main(void) {
    const char* json = "[{\"open\":100,\"high\":105,\"low\":98,\"close\":103}]";
    cc_ohlc_t* ohlc = NULL;
    int size = 0;
    cc_json_to_ohlc(json, &ohlc, &size);

    cc_settings_t s = { .rise_color = CC_COLOR_BLUE, .show_prices = 1, .show_times = 1 };
    char* chart = cc_line_create(ohlc, size, 60, 8, &s);
    printf("%s\n", chart);

    free(chart);
    free(ohlc);
    return 0;
}
```

### Python
```python
from ccharts import Chart

# OHLC Chart
chart = Chart.from_arrays(opens, highs, lows, closes, ts=epoch_seconds)
print(chart.candle(width=60, height=8, show_prices=True))

# Standalone Visuals
print(Chart.pie(["Rent", "Food", "Tech"], [45, 30, 25], donut=True))
print(Chart.histogram(samples, bin_count=10, show_bins=True))
print(Chart.sparkline(samples, height=1))
```

### Rust
```rust
use ccharts::{Chart, Color, Settings, PieSlice, PieOptions};

let chart = Chart::from_arrays(&opens, &highs, &lows, &closes, Some(&ts))?;
println!("{}", chart.line(60, 8, &Settings::new().rise(Color::Blue))?);

let slices = [PieSlice::new(Some("Rent"), 45.0), PieSlice::new(Some("Food"), 30.0)];
println!("{}", Chart::pie(&slices, 24, 10, &PieOptions::new().donut(true))?);
```

### Go
```go
chart, _ := ccharts.FromArrays(opens, highs, lows, closes, ts)
defer chart.Close()
out, _ := chart.Candle(60, 8, &ccharts.Options{ShowPrices: true})
fmt.Println(out)
```

### JavaScript / WASM
```javascript
import { Chart, Color } from "@dethrandir/ccharts";

const chart = Chart.fromArrays(opens, highs, lows, closes);
console.log(chart.candle({ width: 60, height: 8, showPrices: true }));
chart.free();
```

### .NET / C#
```csharp
using var chart = Chart.FromArrays(opens, highs, lows, closes);
Console.Write(chart.Line(new ChartOptions { RiseColor = Color.Blue, ShowPrices = true }));
```

### Java (JDK 22+)
```java
try (Chart chart = Chart.fromArrays(opens, highs, lows, closes)) {
    System.out.print(chart.candle(ChartOptions.builder().size(60, 8).showPrices(true).build()));
}
```

### Ruby
```ruby
chart = Ccharts::Chart.from_arrays(open: opens, high: highs, low: lows, close: closes)
puts chart.line(60, 8, Ccharts::Settings::ChartSettings.new.show_prices(true))
```

### Lua
```lua
local chart = ccharts.from_arrays(opens, highs, lows, closes)
print(chart:candle(60, 8, { show_prices = true }))
```

### Julia
```julia
chart = Chart.from_arrays(opens, highs, lows, closes)
println(chart.candle(60, 8; show_prices=true))
```

---

## Settings & Colors

| Field | Meaning | Default |
| :--- | :--- | :--- |
| `rise_color` | Color for rising segments / bars / boxes | Green (`\x1b[32m`) |
| `fall_color` | Color for falling segments / candles | Red (`\x1b[31m`) |
| `bg_color` | Background of empty cells | `None` (terminal default) |
| `area_color` | Line fill color / Whiskers color | `None` |
| `single_color`| 1 = one uniform line color; 0 = per-segment direction | 0 |
| `show_prices` | Prepend 8-column price/count value axis | 0 |
| `show_times` | Append first/last timestamp footer | 0 |
| `plain` | Strip all ANSI escape sequences | 0 |

Setting `plain=True` (or passing empty strings for color options) completely strips ANSI escape codes, rendering clean plain text ideal for file exports, commit comments, Discord code blocks, and logs.

---

## Detailed Documentation

Comprehensive guides are available in the [`docs/`](docs/) directory:

- 🚀 [**Getting Started**](docs/getting-started.md) — Installation, building from source, and basic usage across all languages.
- 📊 [**Chart Types Guide**](docs/chart-types.md) — In-depth parameters, sub-pixel rendering mechanics, and visual outputs for all 9 chart types.
- 🔌 [**Language Bindings Guide**](docs/bindings.md) — Package managers, memory management (RAII/Cleaners/Finalizers), and cross-language API mappings.
- 🛠️ [**C API & Flat ABI Reference**](docs/c-api.md) — Single-header macros, structs, functions, and ABI status code contracts.
- 🏗️ [**Architecture & Engine Design**](docs/architecture.md) — Sub-pixel grid pipeline, downsampling math, 32-byte slot memory model, and determinism.

---

## Repository Structure

- `ccharts.h` — Core single-header C library containing all rendering algorithms.
- `abi/` — Flat C ABI (`ccharts_abi.h`/`.c`) exporting opaque handles and error codes for FFIs.
- `bindings/` — Idiomatic bindings:
  - `bindings/rust/` — Rust crate (`ccharts`).
  - `bindings/go/` — Go module (`ccharts`).
  - `bindings/js/` — Standalone WebAssembly npm package (`@dethrandir/ccharts`).
  - `bindings/dotnet/` — .NET 8+ P/Invoke package (`Ccharts`).
  - `bindings/java/` — Java 22+ FFM API library (`io.github.dethrandir:ccharts`).
  - `bindings/ruby/` — Ruby gem (`ccharts`).
  - `bindings/lua/` — LuaRocks package (`ccharts`).
  - `bindings/julia/` — Julia package (`Ccharts`).
- `ccharts/` — Python package (`ccharts`) wrapping C core + pandas integration.
- `docs/` — Modular documentation guides.
- `conformance/` — 70 cross-language test cases (`cases.json`) and byte-for-byte golden files (`golden/*.txt`).
- `scripts/` — Version consistency checks, source sync tools, and golden generators.

---

## License

`ccharts` is released under the [MIT License](LICENSE).