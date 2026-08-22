# Ccharts

Terminal charts for financial OHLC data — line and candlestick charts as
ANSI-colored strings of Unicode block characters, ready to write to the
console.

.NET bindings for the C library [ccharts](https://github.com/dethrandir/ccharts),
using source-generated P/Invoke (`[LibraryImport]`) over its flat C ABI.

```sh
dotnet add package Ccharts
```

```csharp
using Ccharts;

using var chart = Chart.FromArrays(open, high, low, close, epochSeconds);

Console.Write(chart.Line(new ChartOptions
{
    Width = 60,
    Height = 8,
    RiseColor = Color.Blue,
    ShowPrices = true,
    ShowTimes = true,
}));
Console.Write(chart.Candle(new ChartOptions { Width = 60, Height = 8 }));
```

- Data can also come from `Chart.FromJson` (fixed-schema JSON) or
  `Chart.FromCsv`.
- `Color` exposes the sixteen ANSI colors; any escape string works, so
  256-color and truecolor do too.
- A `Chart` is immutable once built and safe to render from several threads.
- `Dispose` releases the native memory; a finalizer covers a forgotten call.

Errors are `CchartsException` with a `Status` (`InvalidArgument`, `Parse`,
`OutOfMemory`, `NonFinite`, `Dimensions`).

## The native library

The published package carries `runtimes/{rid}/native/`, so nothing has to be
installed. When working inside the repository the library comes from the CMake
build instead:

```sh
cmake -S . -B build && cmake --build build
dotnet test bindings/dotnet/tests/Ccharts.Tests
```

`NativeLibraryResolver` probes, in order: the default runtime resolution (the
NuGet layout), `CCHARTS_NATIVE_DIR`, and then `build/` directories above the
assembly.

## License

MIT — see the [repository](https://github.com/dethrandir/ccharts).
