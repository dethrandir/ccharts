using System.Runtime.InteropServices;

namespace Ccharts;

/// <summary>
/// A parsed OHLC dataset that can be rendered as a line or candlestick chart.
/// </summary>
/// <remarks>
/// <para>
/// Charts are immutable once built and the native library holds no mutable
/// global state, so an instance is safe to render from several threads.
/// </para>
/// <para>
/// <see cref="Dispose"/> releases the native memory; a finalizer covers a
/// forgotten call.
/// </para>
/// <example>
/// <code>
/// using var chart = Chart.FromArrays(open, high, low, close, epochSeconds);
/// Console.Write(chart.Line(new ChartOptions { Width = 60, ShowPrices = true }));
/// </code>
/// </example>
/// </remarks>
public sealed class Chart : IDisposable
{
    private static readonly ChartOptions DefaultOptions = new();

    private IntPtr _handle;
    private readonly int _length;

    private Chart(IntPtr handle)
    {
        _handle = handle;
        _length = NativeMethods.DataLength(handle);
    }

    /// <summary>
    /// Builds a chart from four equal-length price columns.
    /// </summary>
    /// <param name="open">Opening prices.</param>
    /// <param name="high">High prices.</param>
    /// <param name="low">Low prices.</param>
    /// <param name="close">Closing prices.</param>
    /// <param name="ts">
    /// Epoch seconds, or an empty span when the candles have no timestamps.
    /// </param>
    /// <exception cref="CchartsException">
    /// The columns were empty, of differing lengths, or held NaN/infinity.
    /// </exception>
    public static Chart FromArrays(
        ReadOnlySpan<double> open, ReadOnlySpan<double> high,
        ReadOnlySpan<double> low, ReadOnlySpan<double> close,
        ReadOnlySpan<long> ts = default)
    {
        NativeLibraryResolver.EnsureInstalled();

        var n = open.Length;
        if (n == 0)
        {
            throw new CchartsException(CchartsStatus.InvalidArgument,
                "need at least one candle");
        }
        if (high.Length != n || low.Length != n || close.Length != n)
        {
            throw new CchartsException(CchartsStatus.InvalidArgument,
                "open, high, low and close must have the same length");
        }
        if (!ts.IsEmpty && ts.Length != n)
        {
            throw new CchartsException(CchartsStatus.InvalidArgument,
                "ts must have the same length as the price columns");
        }

        // An empty span marshals to a null pointer, which the ABI reads as
        // "no timestamps".
        var status = NativeMethods.FromArrays(open, high, low, close, ts, n, out var handle);
        CchartsException.ThrowIfError(status);
        return new Chart(handle);
    }

    /// <summary>
    /// Builds a chart from the fixed-schema JSON document: an array of objects
    /// with <c>ts</c>, <c>open</c>, <c>high</c>, <c>low</c> and <c>close</c>.
    /// </summary>
    /// <exception cref="CchartsException">The document could not be parsed.</exception>
    public static Chart FromJson(string json)
    {
        ArgumentNullException.ThrowIfNull(json);
        NativeLibraryResolver.EnsureInstalled();

        var utf8 = Marshal.StringToCoTaskMemUTF8(json);
        try
        {
            CchartsException.ThrowIfError(NativeMethods.ParseJson(utf8, out var handle));
            return new Chart(handle);
        }
        finally
        {
            Marshal.FreeCoTaskMem(utf8);
        }
    }

    /// <summary>
    /// Builds a chart from CSV rows of <c>open,high,low,close[,timestamp]</c>.
    /// Blank lines are skipped.
    /// </summary>
    public static Chart FromCsv(string csv, char valueSeparator = ',', char lineSeparator = '\n')
    {
        ArgumentNullException.ThrowIfNull(csv);
        if (valueSeparator > 127 || lineSeparator > 127 ||
            valueSeparator == '\0' || lineSeparator == '\0')
        {
            throw new CchartsException(CchartsStatus.InvalidArgument,
                "separators must be non-NUL ASCII characters");
        }
        NativeLibraryResolver.EnsureInstalled();

        var utf8 = Marshal.StringToCoTaskMemUTF8(csv);
        try
        {
            var status = NativeMethods.ParseCsv(utf8, (byte)valueSeparator,
                (byte)lineSeparator, out var handle);
            CchartsException.ThrowIfError(status);
            return new Chart(handle);
        }
        finally
        {
            Marshal.FreeCoTaskMem(utf8);
        }
    }

    /// <summary>Number of candles in the dataset.</summary>
    public int Length => _length;

    /// <summary>
    /// Renders a pie/donut chart from the given slices. A pie has no OHLC
    /// dataset, so this is a static method taking the slices directly.
    /// </summary>
    /// <param name="slices">The slices, in the order they are drawn.</param>
    /// <param name="options">Pie options, or <c>null</c> for the defaults.</param>
    /// <returns>The chart as a printable string.</returns>
    /// <exception cref="CchartsException">
    /// The slices were empty, held NaN/infinity, or the dimensions were out of
    /// range.
    /// </exception>
    public static string Pie(IReadOnlyList<PieSlice> slices, PieOptions? options = null)
    {
        ArgumentNullException.ThrowIfNull(slices);
        NativeLibraryResolver.EnsureInstalled();

        options ??= new PieOptions();
        if (slices.Count == 0)
        {
            throw new CchartsException(CchartsStatus.InvalidArgument,
                "need at least one slice");
        }

        var native = new NativePieSlice[slices.Count];
        var labels = new IntPtr[slices.Count];
        try
        {
            for (var i = 0; i < slices.Count; i++)
            {
                labels[i] = slices[i].Label is null ? IntPtr.Zero
                    : Marshal.StringToCoTaskMemUTF8(slices[i].Label);
                native[i] = new NativePieSlice { Label = labels[i], Value = slices[i].Value };
            }

            IntPtr[]? colors = null;
            if (options.Colors is { Count: > 0 })
            {
                colors = new IntPtr[options.Colors.Count];
                for (var i = 0; i < options.Colors.Count; i++)
                {
                    var color = options.Colors[i];
                    colors[i] = string.IsNullOrEmpty(color) ? IntPtr.Zero
                        : Marshal.StringToCoTaskMemUTF8(color);
                }
            }

            try
            {
                var centerText = string.IsNullOrEmpty(options.CenterText) ? IntPtr.Zero
                    : Marshal.StringToCoTaskMemUTF8(options.CenterText);
                try
                {
                    var status = NativeMethods.PieFromSlices(
                        native, slices.Count, options.Width, options.Height,
                        options.Donut ? 1 : 0, colors, colors?.Length ?? 0,
                        options.ShowLegend ? 1 : 0, options.ShowPct ? 1 : 0,
                        options.SliceGap, options.InnerRadiusRatio,
                        options.LegendFormat, options.StartAngle,
                        options.CounterClockwise ? 1 : 0, centerText,
                        out var chart, out var length);
                    CchartsException.ThrowIfError(status);

                    try
                    {
                        return Marshal.PtrToStringUTF8(chart, checked((int)length)) ?? string.Empty;
                    }
                    finally
                    {
                        NativeMethods.StringFree(chart);
                    }
                }
                finally
                {
                    if (centerText != IntPtr.Zero)
                    {
                        Marshal.FreeCoTaskMem(centerText);
                    }
                }
            }
            finally
            {
                if (colors is not null)
                {
                    foreach (var color in colors)
                    {
                        if (color != IntPtr.Zero)
                        {
                            Marshal.FreeCoTaskMem(color);
                        }
                    }
                }
            }
        }
        finally
        {
            foreach (var label in labels)
            {
                if (label != IntPtr.Zero)
                {
                    Marshal.FreeCoTaskMem(label);
                }
            }
        }
    }

    /// <summary>
    /// Renders a histogram of the given scalar samples. A histogram has no
    /// OHLC dataset, so this is a static method taking the raw sample values.
    /// </summary>
    /// <param name="samples">The scalar values to bin.</param>
    /// <param name="options">Histogram options, or <c>null</c> for the defaults.</param>
    /// <returns>The chart as a printable string.</returns>
    /// <exception cref="CchartsException">
    /// The samples were empty or held NaN/infinity, or the dimensions were out
    /// of range.
    /// </exception>
    public static string Histogram(IReadOnlyList<double> samples, HistogramOptions? options = null)
    {
        ArgumentNullException.ThrowIfNull(samples);
        NativeLibraryResolver.EnsureInstalled();

        options ??= new HistogramOptions();
        if (samples.Count == 0)
        {
            throw new CchartsException(CchartsStatus.InvalidArgument,
                "need at least one sample");
        }

        var colors = new IntPtr[2];
        try
        {
            // An empty C string means "emit no escape at all", which is
            // different from a null pointer (use the default color).
            colors[0] = options.Plain ? Empty() : Utf8(options.Color);
            colors[1] = options.Plain ? Empty() : Utf8(options.BackgroundColor);

            var settings = new NativeHistSettings
            {
                RiseColor = colors[0],
                BackgroundColor = colors[1],
                BinCount = options.BinCount,
                // null is the NaN "auto" sentinel: use the data min/max.
                MinValue = options.MinValue ?? double.NaN,
                MaxValue = options.MaxValue ?? double.NaN,
                ShowBins = options.ShowBins ? 1 : 0,
                ShowPrices = options.ShowPrices ? 1 : 0,
            };

            var status = NativeMethods.Hist(samples.ToArray(), samples.Count,
                options.Width, options.Height, in settings, out var chart, out var length);
            CchartsException.ThrowIfError(status);

            try
            {
                return Marshal.PtrToStringUTF8(chart, checked((int)length));
            }
            finally
            {
                NativeMethods.StringFree(chart);
            }
        }
        finally
        {
            foreach (var color in colors)
            {
                if (color != IntPtr.Zero)
                {
                    Marshal.FreeCoTaskMem(color);
                }
            }
        }
    }

    /// <summary>Version of the underlying C library.</summary>
    public static string Version
    {
        get
        {
            NativeLibraryResolver.EnsureInstalled();
            return Marshal.PtrToStringUTF8(NativeMethods.Version()) ?? string.Empty;
        }
    }

    /// <summary>Largest width or height in cells (CC_MAX_DIM).</summary>
    public static int MaxDim
    {
        get
        {
            NativeLibraryResolver.EnsureInstalled();
            return NativeMethods.MaxDim();
        }
    }

    /// <summary>Largest number of cells in a chart (CC_MAX_CELLS).</summary>
    public static int MaxCells
    {
        get
        {
            NativeLibraryResolver.EnsureInstalled();
            return NativeMethods.MaxCells();
        }
    }

    /// <summary>Renders a line chart of the closing prices.</summary>
    /// <exception cref="CchartsException">The dimensions were out of range.</exception>
    public string Line(ChartOptions? options = null) => Render(line: true, options);

    /// <summary>Renders a candlestick chart.</summary>
    /// <exception cref="CchartsException">The dimensions were out of range.</exception>
    public string Candle(ChartOptions? options = null) => Render(line: false, options);

    private string Render(bool line, ChartOptions? options)
    {
        ObjectDisposedException.ThrowIf(_handle == IntPtr.Zero, this);
        options ??= DefaultOptions;

        var colors = new IntPtr[4];
        try
        {
            // An empty C string means "emit no escape at all", which is
            // different from a null pointer (use the default color).
            colors[0] = options.Plain ? Empty() : Utf8(options.RiseColor);
            colors[1] = options.Plain ? Empty() : Utf8(options.FallColor);
            colors[2] = options.Plain ? Empty() : Utf8(options.BackgroundColor);
            colors[3] = options.Plain ? Empty() : Utf8(options.AreaColor);

            var settings = new NativeSettings
            {
                RiseColor = colors[0],
                FallColor = colors[1],
                BackgroundColor = colors[2],
                AreaColor = colors[3],
                SingleColor = options.SingleColor ? 1 : 0,
                ShowPrices = options.ShowPrices ? 1 : 0,
                ShowTimes = options.ShowTimes ? 1 : 0,
            };

            var status = line
                ? NativeMethods.Line(_handle, options.Width, options.Height, in settings,
                    out var chart, out var length)
                : NativeMethods.Candle(_handle, options.Width, options.Height, in settings,
                    out chart, out length);
            CchartsException.ThrowIfError(status);

            try
            {
                return Marshal.PtrToStringUTF8(chart, checked((int)length));
            }
            finally
            {
                NativeMethods.StringFree(chart);
            }
        }
        finally
        {
            foreach (var color in colors)
            {
                if (color != IntPtr.Zero)
                {
                    Marshal.FreeCoTaskMem(color);
                }
            }
        }
    }

    private static IntPtr Utf8(string? value) =>
        string.IsNullOrEmpty(value) ? IntPtr.Zero : Marshal.StringToCoTaskMemUTF8(value);

    private static IntPtr Empty() => Marshal.StringToCoTaskMemUTF8(string.Empty);

    /// <summary>Releases the native dataset. Safe to call more than once.</summary>
    public void Dispose()
    {
        var handle = Interlocked.Exchange(ref _handle, IntPtr.Zero);
        if (handle != IntPtr.Zero)
        {
            NativeMethods.DataFree(handle);
        }
        GC.SuppressFinalize(this);
    }

    /// <summary>Releases the native dataset if <see cref="Dispose"/> was not called.</summary>
    ~Chart() => Dispose();
}
