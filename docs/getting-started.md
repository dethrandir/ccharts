# Getting Started with ccharts

`ccharts` is an ultra-fast, zero-dependency charting library that renders financial and statistical charts directly as Unicode strings. There is no canvas, no image buffer, and no GUI or terminal hooks—data goes in, a formatted UTF-8 string comes out.

---

## Key Principles

- **Strings out, not streams**: Every function returns an allocated string (`char*`, `str`, `String`, etc.). You decide where to send it (`printf`, loggers, web APIs, Discord bots, terminal output, files).
- **Zero runtime dependencies**: Written in clean, portable C89 with a flat C ABI.
- **Deterministic & Byte-Identical**: The same inputs and dimensions produce the exact same bytes across all 9 language bindings.
- **ANSI Color or Plain Text**: Full support for ANSI colors, 256-color, truecolor, or 100% plain text (`plain=true`) without escape codes for logs and files.

---

## Quickstart by Language

### C (Single-Header)

`ccharts.h` is a single-header library. Define `CCHARTS_IMPLEMENTATION` in exactly one translation unit before including it:

```c
#define CCHARTS_IMPLEMENTATION
#include "ccharts.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    const char* json = "[{\"open\":100,\"high\":105,\"low\":98,\"close\":103}]";
    cc_ohlc_t* ohlc = NULL;
    int size = 0;
    
    if (cc_json_to_ohlc(json, &ohlc, &size) == 0) {
        cc_settings_t s = { .rise_color = CC_COLOR_GREEN, .show_prices = 1 };
        char* chart = cc_candle_create(ohlc, size, 60, 8, &s);
        
        printf("%s\n", chart);
        
        free(chart);
        free(ohlc);
    }
    return 0;
}
```
Compile with:
```sh
gcc -O3 main.c -lm -o demo
```

---

### Python

Install via PyPI:
```sh
pip install ccharts
# Optional pandas DataFrame support:
pip install ccharts[pandas]
```

Usage:
```python
from ccharts import Chart

# OHLC Candlestick chart from raw arrays
chart = Chart.from_arrays(
    open=[100.0, 102.5, 101.0, 104.0],
    high=[103.0, 105.0, 103.5, 106.0],
    low=[99.0, 101.0, 99.5, 102.0],
    close=[102.0, 101.5, 103.5, 105.5]
)
print(chart.candle(width=60, height=8, show_prices=True))

# Standalone chart (e.g. Pie chart)
print(Chart.pie(["Rent", "Food", "Tech"], [45, 30, 25], donut=True))
```

---

### Rust

Add to your `Cargo.toml`:
```toml
[dependencies]
ccharts = "3.0.0"
```

Usage:
```rust
use ccharts::{Chart, Color, Settings};

fn main() -> Result<(), ccharts::Error> {
    let opens = [100.0, 102.5, 101.0, 104.0];
    let highs = [103.0, 105.0, 103.5, 106.0];
    let lows  = [99.0, 101.0, 99.5, 102.0];
    let closes= [102.0, 101.5, 103.5, 105.5];

    let chart = Chart::from_arrays(&opens, &highs, &lows, &closes, None)?;
    let settings = Settings::new().rise(Color::Green).show_prices(true);

    println!("{}", chart.candle(60, 8, &settings)?);
    Ok(())
}
```

---

### Go

Install the Go module:
```sh
go get github.com/dethrandir/ccharts/bindings/go/ccharts
```

Usage:
```go
package main

import (
	"fmt"
	"log"
	"github.com/dethrandir/ccharts/bindings/go/ccharts"
)

func main() {
	opens := []float64{100.0, 102.5, 101.0, 104.0}
	highs := []float64{103.0, 105.0, 103.5, 106.0}
	lows := []float64{99.0, 101.0, 99.5, 102.0}
	closes := []float64{102.0, 101.5, 103.5, 105.5}

	chart, err := ccharts.FromArrays(opens, highs, lows, closes, nil)
	if err != nil {
		log.Fatal(err)
	}
	defer chart.Close()

	out, err := chart.Candle(60, 8, &ccharts.Options{ShowPrices: true})
	if err != nil {
		log.Fatal(err)
	}
	fmt.Println(out)
}
```

---

### JavaScript / TypeScript (Node.js, Deno, Bun, Browser)

Install via npm:
```sh
npm install @dethrandir/ccharts
```

Usage:
```javascript
import { Chart, Color } from "@dethrandir/ccharts";

const opens = [100.0, 102.5, 101.0, 104.0];
const highs = [103.0, 105.0, 103.5, 106.0];
const lows  = [99.0, 101.0, 99.5, 102.0];
const closes= [102.0, 101.5, 103.5, 105.5];

const chart = Chart.fromArrays(opens, highs, lows, closes);
console.log(chart.candle({ width: 60, height: 8, showPrices: true }));
chart.free();
```

---

### .NET / C#

Install via NuGet:
```sh
dotnet add package Ccharts
```

Usage:
```csharp
using System;
using Ccharts;

double[] opens = [100.0, 102.5, 101.0, 104.0];
double[] highs = [103.0, 105.0, 103.5, 106.0];
double[] lows = [99.0, 101.0, 99.5, 102.0];
double[] closes = [102.0, 101.5, 103.5, 105.5];

using var chart = Chart.FromArrays(opens, highs, lows, closes);
Console.Write(chart.Candle(new ChartOptions { Width = 60, Height = 8, ShowPrices = true }));
```

---

### Java (JDK 22+)

Add Maven dependency:
```xml
<dependency>
  <groupId>io.github.dethrandir</groupId>
  <artifactId>ccharts</artifactId>
  <version>3.0.0</version>
</dependency>
```

Usage:
```java
import io.github.dethrandir.ccharts.*;

public class Main {
    public static void main(String[] args) {
        double[] opens = {100.0, 102.5, 101.0, 104.0};
        double[] highs = {103.0, 105.0, 103.5, 106.0};
        double[] lows = {99.0, 101.0, 99.5, 102.0};
        double[] closes = {102.0, 101.5, 103.5, 105.5};

        try (Chart chart = Chart.fromArrays(opens, highs, lows, closes)) {
            System.out.print(chart.candle(ChartOptions.builder()
                .size(60, 8)
                .showPrices(true)
                .build()));
        }
    }
}
```

---

### Ruby

Install via RubyGems:
```sh
gem install ccharts
```

Usage:
```ruby
require "ccharts"

chart = Ccharts::Chart.from_arrays(
  open: [100.0, 102.5, 101.0, 104.0],
  high: [103.0, 105.0, 103.5, 106.0],
  low: [99.0, 101.0, 99.5, 102.0],
  close: [102.0, 101.5, 103.5, 105.5]
)

settings = Ccharts::Settings::ChartSettings.new.show_prices(true)
puts chart.candle(60, 8, settings)
```

---

### Lua

Install via LuaRocks:
```sh
luarocks install ccharts
```

Usage:
```lua
local ccharts = require "ccharts"

local chart = ccharts.from_arrays(
  {100.0, 102.5, 101.0, 104.0},
  {103.0, 105.0, 103.5, 106.0},
  {99.0, 101.0, 99.5, 102.0},
  {102.0, 101.5, 103.5, 105.5}
)

print(chart:candle(60, 8, { show_prices = true }))
```

---

### Julia

Install via Julia package manager:
```julia
using Pkg
Pkg.add("Ccharts")
```

Usage:
```julia
using Ccharts

opens = [100.0, 102.5, 101.0, 104.0]
highs = [103.0, 105.0, 103.5, 106.0]
lows  = [99.0, 101.0, 99.5, 102.0]
closes= [102.0, 101.5, 103.5, 105.5]

chart = Chart.from_arrays(opens, highs, lows, closes)
println(chart.candle(60, 8, show_prices=true))
```

---

## Building from Source

To build and test the core C library, shared ABI, and bindings locally:

### 1. Build Core C ABI & Smoke Tests
```sh
cmake -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

### 2. Build and Test Python Binding
```sh
python3 -m pip install -e ".[pandas]"
python3 -m unittest discover -s tests -v
```

### 3. Run Cross-Language Conformance Suite
```sh
# Generate or verify golden outputs
python3 scripts/gen_golden.py --check

# Check source synchronization across vendored copies
python3 scripts/sync_sources.py --check

# Check version consistency
python3 scripts/check_versions.py
```

---

## Next Steps

- Explore all supported visual types: [Chart Types Guide](chart-types.md)
- Learn about language bindings and memory safety: [Bindings Guide](bindings.md)
- Understand the underlying C API & ABI: [C API Reference](c-api.md)
- Explore the rendering engine architecture: [Architecture & Pipeline](architecture.md)
