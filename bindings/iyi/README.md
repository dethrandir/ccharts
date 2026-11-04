# ccharts for iyi

An owning iyi binding to ccharts' existing flat C ABI. All nine chart families
are supported: line, candlestick, pie/donut, histogram, sparkline, bar, stacked
bar, heatmap and box plot. Rendering stays in `ccharts.h`; iyi handles typed
inputs, options, error values and native ownership.

Requires iyi **0.20.1**, Python 3 (development helper), CMake and a C compiler.
This binding targets 64-bit native hosts. Linux x86-64 and Windows x86-64
passed the full iyi CI job. The macOS arm64 job is configured but still queued;
see the validation report for the exact run and tested commit. Cross-compilation,
32-bit targets and WebAssembly have not been qualified for this binding.
There is no runtime Python dependency and no `--crystal` requirement.

## Build and try it

From the repository root:

```sh
python3 bindings/iyi/scripts/dev.py test
python3 bindings/iyi/scripts/dev.py cli
bindings/iyi/build/ccharts-market --help
fl market history THYAO 1m 1d --json | bindings/iyi/build/ccharts-market
fl price ASELS --json | bindings/iyi/build/ccharts-market --type line
```

On Windows use `python` and `bindings/iyi/build/ccharts-market.exe`; the native
build needs the Visual Studio C++ tools. The helper builds a static archive from
the vendored ABI sources and sets `IYI_LIBRARY_PATH` for its child processes.
It does not install software or fetch financial data. See the
[CLI guide](examples/README.md) for offline fixtures, options and input schemas.

To build the native archive without Python:

```sh
cmake -S bindings/iyi -B bindings/iyi/build -DCMAKE_BUILD_TYPE=Release
cmake --build bindings/iyi/build --config Release
export IYI_LIBRARY_PATH="$PWD/bindings/iyi/build"
iyi run bindings/iyi/examples/quickstart.iyi
```

`IYI_LIBRARY_PATH` is a build-time setting. The resulting executable carries the
static ccharts archive and needs no separate ccharts shared library at runtime.
It still uses the platform's C runtime and, on Unix, math library.

## Use from another iyi project

The package manifest is at the repository root, as required by `iyi get`.
The canonical identity is `github.com/dethrandir/ccharts/v3`: iyi requires the
major suffix for ccharts' shared 3.x version. Importing it exposes `Ccharts`.

**The iyi binding is available from `main`; a tagged release is pending.**
Existing release tags predate the iyi package. Select the branch explicitly:

```sh
iyi get github.com/dethrandir/ccharts/v3@main
```

For development before publishing, add this to your application's `iyi.mod`,
replacing the local path with your checkout:

```text
module example.com/my-app
require github.com/dethrandir/ccharts/v3 v3.0.0
replace github.com/dethrandir/ccharts/v3 => ../ccharts
```

Build the native archive from that checkout using the CMake commands above and
point `IYI_LIBRARY_PATH` at its `bindings/iyi/build` directory when compiling
your app. `iyi get` intentionally does not run native build hooks; do not build
inside its immutable module cache. A future tagged ccharts release containing
these files can be selected with `iyi get ...@v<release>`.

```ruby
module main

import github.com/dethrandir/ccharts/v3::{Chart, Settings}

def draw : Nil
  chart = Chart.from_arrays(
    [100.0, 102.0], [105.0, 106.0], [98.0, 101.0], [103.0, 104.0]
  ).or_panic
  defer chart.close
  print chart.candle(40, 8, Settings.new(show_prices: true)).or_panic
end

draw
```

## API and ownership

`Chart.from_arrays(open, high, low, close, timestamps = nil)`,
`Chart.from_json(text)` and `Chart.from_csv(text, value_separator = 44_u8,
line_separator = 10_u8)` return `Chart | ChartError`. Price columns are
`Array(Float64)` and optional timestamps are `Array(Int64)` epoch seconds.
Native input data is copied immediately; subsequent input mutations do not
change the dataset.

A chart owns its native dataset. **Call `chart.close` or use `defer chart.close`
in a function.** There is no GC finalizer. Closing twice is safe; aliases refer
to the same closed state, and rendering after close returns `ChartError`.
Concurrent rendering and closing of the same chart requires caller
coordination. Rendered strings are copied into iyi memory and the C allocation
is freed immediately, so strings remain valid after the chart closes.

| Renderer | Settings / inputs |
| --- | --- |
| `chart.line(width, height, settings)` | `Settings` |
| `chart.candle(width, height, settings)` | `Settings` |
| `Chart.pie(slices, width, height, settings)` | `Array(Slice)`, `PieSettings` |
| `Chart.histogram(samples, width, height, settings)` | `Array(Float64)`, `HistSettings` |
| `Chart.sparkline(samples, width, height = 1, settings)` | `Array(Float64)`, `SparkSettings` |
| `Chart.bar(items, width, height, settings)` | `Array(Slice)`, `BarSettings` |
| `Chart.stacked_bar(series, width, height, settings)` | `Array(Series)`, `StackSettings` |
| `Chart.heatmap(rows, width, height, settings)` | `Array(Array(Float64))`, `HeatSettings` |
| `Chart.boxplot(categories, width, height, settings)` | `Array(Category)`, `BoxSettings` |

Every settings type has named constructor parameters and `plain: true` to
suppress ANSI colors. Defaults match the C ABI. Colors are ANSI strings;
`Charts.color(index)` provides the shared palette (0–15, reset at 16).
`Charts.version`, `Charts.max_dim`, and `Charts.max_cells` expose native metadata.
`Charts.hist/spark/pie/bar/stack/heat/box` are equivalent stateless shortcuts.
`Slice.new(label, value)`, `Series.new(name, values)` and
`Category.new(name, samples)` describe categorical inputs.

Renderers return `String | ChartError`, with the C status preserved as `code`
and text in `message`. The binding also checks mismatched column lengths,
ragged grids, label counts and embedded NUL strings before crossing the ABI.
Non-finite values and invalid dimensions are rejected by the existing ABI.
Use `.or_panic` for small demos or handle `ChartError` explicitly in applications.

## Verification and publication

`python3 bindings/iyi/scripts/dev.py test` runs the native build, API tests, all
70 shared byte-for-byte conformance cases, the actual CLI black-box tests and
a separate package consumer. The consumer uses `iyi get` against an isolated
temporary git mirror, verifies `iyi.sum`, and rebuilds after removing the mirror.
No network or Florence account is required for these tests.

The shared version guard and vendor drift guard include this binding. Update
C originals and run `python3 scripts/sync_sources.py`; do not edit `vendor/`.
The [validation report](VALIDATION.md) records the tested environment and limits.

[iyipacks](https://packs.iyi.dev/about/index.html) is a discovery catalog, not an
upload registry. Its documented requirements are a public GitHub repository,
a root `iyi.mod`, and either the `iyi` topic or code-search discovery of the
manifest. The current importer also requires an exact repository-path match,
so its `/v3` handling needs a fix before this package can be listed. See
[publication notes](PUBLICATION.md). Inclusion has not been observed yet.
