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
            colors[0] = Utf8(options.RiseColor);
            colors[1] = Utf8(options.FallColor);
            colors[2] = Utf8(options.BackgroundColor);
            colors[3] = Utf8(options.AreaColor);

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
