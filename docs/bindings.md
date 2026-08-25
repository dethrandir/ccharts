# Language Bindings Guide

`ccharts` provides idiomatic bindings for **9 major programming languages**. Every binding wraps the same core C engine, ensuring identical, byte-for-byte visual output across all platforms and runtimes.

---

## Ecosystem Overview

| Language | Ecosystem / Registry | Target / Package | Integration Mechanism | Binary / Compilation |
| :--- | :--- | :--- | :--- | :--- |
| **Python** | PyPI | `ccharts` | CPython C-Extension | Pre-built wheels + sdist |
| **Rust** | crates.io | `ccharts` | Rust FFI (`cc` crate) | Compiles vendored C at build time |
| **Go** | Go Modules | `github.com/dethrandir/ccharts/bindings/go/ccharts` | cgo | Compiles vendored C via cgo |
| **JavaScript** | npm | `@dethrandir/ccharts` | WebAssembly (ESM) | Embedded standalone `.wasm` (no deps) |
| **.NET / C#** | NuGet | `Ccharts` | Source-gen P/Invoke (`[LibraryImport]`) | Ships prebuilt runtime binaries |
| **Java** | Maven Central | `io.github.dethrandir:ccharts` | FFM API (`java.lang.foreign`, JDK 22+) | Ships prebuilt native shared libs |
| **Ruby** | RubyGems | `ccharts` | `mkmf` + `Fiddle` (libffi) | Compiles C ABI at install time |
| **Lua** | LuaRocks | `ccharts` | Lua C module + pure Lua facade | Compiles C ABI at install time |
| **Julia** | Julia General | `Ccharts` | `deps/build.jl` + `ccall` | Compiles C ABI via `deps/build.jl` |

---

## 1. Python (`ccharts`)

- **Package**: `ccharts` on [PyPI](https://pypi.org/project/ccharts)
- **Install**: `pip install ccharts` (or `pip install ccharts[pandas]`)
- **Integration**: A compiled CPython extension module (`ccharts._core` built from `ccharts/wrapper.c`).
- **Memory Management**: The C `cc_ohlc_t*` pointer is wrapped in a `"ccharts.ohlc"` `PyCapsule`. When the Python `Chart` object is garbage collected, the capsule destructor automatically invokes `free()`.
- **Columnar & pandas Support**: `Chart.from_arrays` accepts NumPy arrays, Python lists, or standard buffer-protocol objects. `Chart.from_dataframe` converts pandas DataFrames with zero intermediate JSON serialization; `pandas` is imported lazily.

```python
from ccharts import Chart

chart = Chart.from_arrays(open, high, low, close, ts=epoch_seconds)
print(chart.candle(width=60, height=8, show_prices=True))

# Standalone renderers
print(Chart.pie(["Alpha", "Beta"], [60, 40], donut=True))
print(Chart.histogram(samples, bin_count=10, show_bins=True))
print(Chart.sparkline(samples, height=1))
```

---

## 2. Rust (`ccharts`)

- **Crate**: `ccharts` on [crates.io](https://crates.io/crates/ccharts)
- **Install**: `cargo add ccharts`
- **Integration**: `build.rs` compiles vendored `ccharts.h` and `ccharts_abi.c` using the standard `cc` crate. No dynamic linker dependencies.
- **Memory Management**: Fully RAII compliant. `Chart` holds an opaque pointer to `ccharts_data`. Implementing `Drop` ensures `ccharts_data_free` is called when `Chart` falls out of scope. `Chart` is `Send + Sync`.
- **Render Output**: Native UTF-8 `String` returned using `ccharts_string_free` under the hood.

```rust
use ccharts::{Chart, Color, Settings, PieSlice, PieOptions};

let chart = Chart::from_arrays(&open, &high, &low, &close, Some(&ts))?;
println!("{}", chart.line(60, 8, &Settings::new().rise(Color::Blue))?);

let slices = [PieSlice::new(Some("A"), 50.0), PieSlice::new(Some("B"), 50.0)];
println!("{}", Chart::pie(&slices, 24, 10, &PieOptions::new().donut(true))?);
```

---

## 3. Go (`ccharts`)

- **Module**: `github.com/dethrandir/ccharts/bindings/go/ccharts`
- **Install**: `go get github.com/dethrandir/ccharts/bindings/go/ccharts`
- **Integration**: Built with **cgo**. The C ABI source files are vendored inside the module directory. Requires `CGO_ENABLED=1`.
- **Memory Management**: `Chart` wraps the native pointer. Callers should use `defer chart.Close()` to immediately free C allocations. A `runtime.SetFinalizer` serves as a safety net.

```go
package main

import (
	"fmt"
	"github.com/dethrandir/ccharts/bindings/go/ccharts"
)

func main() {
	chart, _ := ccharts.FromArrays(open, high, low, close, ts)
	defer chart.Close()

	out, _ := chart.Candle(60, 8, &ccharts.Options{ShowPrices: true})
	fmt.Println(out)
}
```

---

## 4. JavaScript / TypeScript (`@dethrandir/ccharts`)

- **Package**: `@dethrandir/ccharts` on [npm](https://www.npmjs.com/package/@dethrandir/ccharts)
- **Install**: `npm install @dethrandir/ccharts`
- **Integration**: Compiled to standalone WebAssembly (`ccharts.wasm`) and base64-inlined into `src/wasm-binary.js`. Zero external dependencies. Uses top-level await for instant ESM initialization across Node.js (18+), Deno, Bun, and web browsers.
- **Memory Management**: Explicit `chart.free()` frees the underlying WebAssembly linear memory. A `FinalizationRegistry` cleans up forgotten handles upon garbage collection.

```typescript
import { Chart, Color } from "@dethrandir/ccharts";

const chart = Chart.fromArrays(open, high, low, close, ts);
console.log(chart.candle({ width: 60, height: 8, showPrices: true }));
chart.free();
```

---

## 5. .NET / C# (`Ccharts`)

- **Package**: `Ccharts` on [NuGet](https://www.nuget.org/packages/Ccharts)
- **Install**: `dotnet add package Ccharts`
- **Integration**: Built on .NET 8+ source-generated P/Invoke (`[LibraryImport]`) over `libccharts_abi`. Prebuilt native binaries for `linux-x64`, `osx-arm64`, and `win-x64` are bundled in `runtimes/{rid}/native/`.
- **Memory Management**: Implements `IDisposable`. Use standard C# `using var chart = ...` syntax. A GC finalizer handles missed disposals.

```csharp
using System;
using Ccharts;

using var chart = Chart.FromArrays(open, high, low, close, ts);
Console.Write(chart.Line(new ChartOptions { Width = 60, Height = 8, ShowPrices = true }));
```

---

## 6. Java (`io.github.dethrandir:ccharts`)

- **Artifact**: `io.github.dethrandir:ccharts` on Maven Central
- **Requirement**: **JDK 22 or newer** (uses Java Foreign Function & Memory API, `java.lang.foreign`). **Zero JNI code**.
- **Native Binaries**: Bundled in jar under `native/{os}-{arch}/` and extracted dynamically by `Native.java`.
- **Memory Management**: Implements `AutoCloseable`. Use in `try-with-resources` blocks. `java.lang.ref.Cleaner` serves as fallback.

```java
import io.github.dethrandir.ccharts.*;

try (Chart chart = Chart.fromArrays(open, high, low, close, ts)) {
    System.out.print(chart.candle(ChartOptions.builder()
        .size(60, 8)
        .rise(Color.GREEN)
        .showPrices(true)
        .build()));
}
```

---

## 7. Ruby (`ccharts`)

- **Gem**: `ccharts` on [RubyGems](https://rubygems.org/gems/ccharts)
- **Install**: `gem install ccharts`
- **Integration**: `extconf.rb` compiles the vendored C ABI into a native shared extension `ccharts_ext.so` at install time. Driven seamlessly via standard library `Fiddle` (libffi wrapper).
- **Memory Management**: `ObjectSpace.define_finalizer` binds the dataset handle to `ccharts_data_free`. Rendered strings are converted to frozen UTF-8 strings and native C buffers are released immediately with `ccharts_string_free`.

```ruby
require "ccharts"

chart = Ccharts::Chart.from_arrays(open: opens, high: highs, low: lows, close: closes)
puts chart.candle(60, 8, Ccharts::Settings::ChartSettings.new.show_prices(true))

# Standalone chart
puts Ccharts::Chart.pie([{ label: "A", value: 40 }, { label: "B", value: 60 }], 24, 10)
```

---

## 8. Lua (`ccharts`)

- **Rock**: `ccharts` on [LuaRocks](https://luarocks.org/modules/dethrandir/ccharts)
- **Install**: `luarocks install ccharts`
- **Integration**: A compiled C module (`ccharts_core.c` + `vendor/ccharts_abi.c`) paired with a pure-Lua facade (`ccharts.lua`). Compatible with Lua 5.1, 5.2, 5.3, 5.4, and LuaJIT.
- **Memory Management**: Opaque `ccharts_data` handles are wrapped in Lua full `userdata` with a `__gc` metamethod calling `ccharts_data_free`.

```lua
local ccharts = require "ccharts"

local chart = ccharts.from_arrays(open, high, low, close, ts)
print(chart:candle(60, 8, { show_prices = true }))

-- Standalone chart
print(ccharts.pie({{label = "A", value = 40}, {label = "B", value = 60}}, 24, 10, { donut = true }))
```

---

## 9. Julia (`Ccharts`)

- **Package**: `Ccharts` on the General Registry
- **Install**: `using Pkg; Pkg.add("Ccharts")`
- **Integration**: `deps/build.jl` compiles the vendored C ABI into a shared library. Directly driven via `ccall`. Contiguous struct arrays (`KPieSlice`, `KBarSlice`, `KStackSeries`, `KBoxCategory`) match C struct layout byte-for-byte.
- **Memory Management**: `Chart` encapsulates a `DataHandle` mutable struct registered with `Base.finalizer` calling `ccharts_data_free`.

```julia
using Ccharts

chart = Chart.from_arrays(open, high, low, close; ts=timestamps)
println(chart.candle(60, 8; show_prices=true))

# Standalone chart
println(pie([("A", 40.0), ("B", 60.0)], 24, 10; donut=true))
```

---

## Cross-Language API Mapping

| Chart / Operation | C ABI | Python | Rust | Go | JavaScript | .NET / C# | Java | Ruby | Lua | Julia |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **From Arrays** | `ccharts_from_arrays` | `Chart.from_arrays` | `Chart::from_arrays` | `FromArrays` | `Chart.fromArrays` | `Chart.FromArrays` | `Chart.fromArrays` | `Chart.from_arrays` | `from_arrays` | `from_arrays` |
| **From JSON** | `ccharts_parse_json` | `Chart(json_str)` | `Chart::from_json` | `FromJSON` | `Chart.fromJson` | `Chart.FromJson` | `Chart.fromJson` | `Chart.from_json` | `from_json` | `from_json` |
| **From CSV** | `ccharts_parse_csv` | — | `Chart::from_csv` | `FromCSV` | `Chart.fromCsv` | `Chart.FromCsv` | `Chart.fromCsv` | `Chart.from_csv` | `from_csv` | `from_csv` |
| **Line Chart** | `ccharts_line` | `chart.line()` | `chart.line()` | `chart.Line()` | `chart.line()` | `chart.Line()` | `chart.line()` | `chart.line()` | `chart:line()` | `chart.line()` |
| **Candle Chart**| `ccharts_candle` | `chart.candle()` | `chart.candle()`| `chart.Candle()`| `chart.candle()`| `chart.Candle()`| `chart.candle()`| `chart.candle()`| `chart:candle()`| `chart.candle()`|
| **Pie / Donut** | `ccharts_pie_from_slices` | `Chart.pie()` | `Chart::pie()` | `Pie()` | `Chart.pie()` | `Chart.Pie()` | `Chart.pie()` | `Chart.pie()` | `pie()` | `pie()` |
| **Histogram** | `ccharts_hist` | `Chart.histogram()` | `Chart::histogram()` | `Histogram()` | `Chart.histogram()` | `Chart.Histogram()` | `Chart.histogram()` | `Chart.histogram()` | `histogram()` | `histogram()` |
| **Sparkline** | `ccharts_spark` | `Chart.sparkline()` | `Chart::sparkline()` | `Sparkline()` | `Chart.sparkline()` | `Chart.Sparkline()` | `Chart.sparkline()` | `Chart.sparkline()` | `sparkline()` | `sparkline()` |
| **Bar** | `ccharts_bar` | `Chart.bar()` | `Chart::bar()` | `Bar()` | `Chart.bar()` | `Chart.Bar()` | `Chart.bar()` | `Chart.bar()` | `bar()` | `bar()` |
| **Stacked Bar** | `ccharts_stack` | `Chart.stacked_bar()` | `Chart::stacked_bar()` | `StackedBar()` | `Chart.stackedBar()` | `Chart.StackedBar()` | `Chart.stackedBar()` | `Chart.stacked_bar()` | `stacked_bar()` | `stacked_bar()` |
| **Heatmap** | `ccharts_heat` | `Chart.heatmap()` | `Chart::heatmap()` | `Heatmap()` | `Chart.heatmap()` | `Chart.Heatmap()` | `Chart.heatmap()` | `Chart.heatmap()` | `heatmap()` | `heatmap()` |
| **Box Plot** | `ccharts_box` | `Chart.boxplot()` | `Chart::boxplot()` | `BoxPlot()` | `Chart.boxPlot()` | `Chart.BoxPlot()` | `Chart.boxPlot()` | `Chart.boxplot()` | `box_plot()` | `boxplot()` |

---

## Memory Management Model

```
                    ┌──────────────────────────────────────────────┐
                    │               User Application               │
                    └──────────────────────┬───────────────────────┘
                                           │
                        ┌──────────────────┴──────────────────┐
                        │      High-Level Language Handle     │
                        │ (RAII / using / try-with / GC hook) │
                        └──────────────────┬──────────────────┘
                                           │
              ┌────────────────────────────┼────────────────────────────┐
              ▼                            ▼                            ▼
   [ Rust / C++ / C# ]             [ Java FFM / Go ]             [ Scripting / GC ]
    RAII `Drop` / Dispose         `AutoCloseable` / `Close`      `Finalizer` / `__gc`
              │                            │                            │
              └────────────────────────────┼────────────────────────────┘
                                           │ Calls
                                           ▼
                    ┌──────────────────────────────────────────────┐
                    │      ccharts_data_free() / string_free()     │
                    └──────────────────────┬───────────────────────┘
                                           │
                                           ▼
                    ┌──────────────────────────────────────────────┐
                    │           Native C Heap Memory (free)        │
                    └──────────────────────────────────────────────┘
```

---

## Conformance Verification

Every binding participates in the **70-case cross-language conformance suite**:
- Test configurations defined in `conformance/cases.json`.
- Golden byte-for-byte references in `conformance/golden/*.txt`.
- Verification tools:
  - `python3 scripts/gen_golden.py --check`
  - `python3 scripts/sync_sources.py --check`
  - `python3 scripts/check_versions.py`
