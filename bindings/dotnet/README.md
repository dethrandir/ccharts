# Ccharts

Turns financial OHLC data into a string — line and candlestick charts drawn
with Unicode block characters. Nothing is printed for you, so the chart goes
wherever text goes: a terminal, a log line, a chat message, an HTML `<pre>`,
a file.

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
- `new ChartOptions { Plain = true }` renders with no ANSI escapes at all, for
  output that is not going to a console.
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
