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

    /// <summary>
    /// Renders a sparkline of the given scalar samples. A sparkline has no
    /// OHLC dataset, so this is a static method taking the raw sample values.
    /// </summary>
    /// <param name="samples">The close-like trend values to draw.</param>
    /// <param name="options">Sparkline options, or <c>null</c> for the defaults.</param>
    /// <returns>The chart as a printable string.</returns>
    /// <exception cref="CchartsException">
    /// The samples were empty or held NaN/infinity, or the dimensions were out
    /// of range.
    /// </exception>
    public static string Sparkline(IReadOnlyList<double> samples, SparklineOptions? options = null)
    {
        ArgumentNullException.ThrowIfNull(samples);
        NativeLibraryResolver.EnsureInstalled();

        options ??= new SparklineOptions();
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
            colors[0] = options.Plain ? Empty() : Utf8(options.RiseColor);
            colors[1] = options.Plain ? Empty() : Utf8(options.AreaColor);

            var settings = new NativeSparkSettings
            {
                RiseColor = colors[0],
                AreaColor = colors[1],
                MinAbove = options.MinAbove,
                MinBelow = options.MinBelow,
            };

            var status = NativeMethods.Spark(samples.ToArray(), samples.Count,
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

    /// <summary>
    /// Renders a categorical bar chart of the <c>(label, value)</c> pairs
    /// formed by the parallel <paramref name="labels"/> and
    /// <paramref name="values"/> lists. A bar chart has no OHLC dataset, so
    /// this is a static method taking the labels and values directly.
    /// </summary>
    /// <param name="labels">The categorical label of each bar.</param>
    /// <param name="values">The height of each bar.</param>
    /// <param name="width">Chart width in cells.</param>
    /// <param name="height">Chart height in cells.</param>
    /// <param name="options">Bar options, or <c>null</c> for the defaults.</param>
    /// <returns>The chart as a printable string.</returns>
    /// <exception cref="CchartsException">
    /// The lists were empty or of differing lengths, held NaN/infinity, or the
    /// dimensions were out of range.
    /// </exception>
    public static string Bar(IReadOnlyList<string> labels, IReadOnlyList<double> values,
        int width, int height, BarOptions? options = null)
    {
        ArgumentNullException.ThrowIfNull(labels);
        ArgumentNullException.ThrowIfNull(values);
        NativeLibraryResolver.EnsureInstalled();

        options ??= new BarOptions();
        if (labels.Count == 0)
        {
            throw new CchartsException(CchartsStatus.InvalidArgument,
                "need at least one bar");
        }
        if (labels.Count != values.Count)
        {
            throw new CchartsException(CchartsStatus.InvalidArgument,
                "labels and values must have the same length");
        }

        var native = new NativeBarSlice[labels.Count];
        var labelPtrs = new IntPtr[labels.Count];
        try
        {
            for (var i = 0; i < labels.Count; i++)
            {
                labelPtrs[i] = Marshal.StringToCoTaskMemUTF8(labels[i]);
                native[i] = new NativeBarSlice { Label = labelPtrs[i], Value = values[i] };
            }

            var colors = new IntPtr[2];
            try
            {
                // An empty C string means "emit no escape at all", which is
                // different from a null pointer (use the default color).
                colors[0] = options.Plain ? Empty() : Utf8(options.Color);
                colors[1] = options.Plain ? Empty() : Utf8(options.BackgroundColor);

                var settings = new NativeBarSettings
                {
                    RiseColor = colors[0],
                    BackgroundColor = colors[1],
                    ShowLabels = options.ShowLabels ? 1 : 0,
                    ShowPrices = options.ShowPrices ? 1 : 0,
                };

                var status = NativeMethods.Bar(native, labels.Count, width, height,
                    in settings, out var chart, out var length);
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
                foreach (var color in colors)
                {
                    if (color != IntPtr.Zero)
                    {
                        Marshal.FreeCoTaskMem(color);
                    }
                }
            }
        }
        finally
        {
            foreach (var label in labelPtrs)
            {
                if (label != IntPtr.Zero)
                {
                    Marshal.FreeCoTaskMem(label);
                }
            }
        }
    }

    /// <summary>
    /// Renders a stacked bar chart of the given series. Each series contributes
    /// one vertical segment per category, and a category's bar height is the SUM
    /// of its series' values. A stacked bar chart has no OHLC dataset, so this
    /// is a static method taking the series directly.
    /// </summary>
    /// <param name="series">
    /// The series, each carrying the same number of values (one per category).
    /// </param>
    /// <param name="width">Chart width in cells.</param>
    /// <param name="height">Chart height in cells.</param>
    /// <param name="options">Stack options, or <c>null</c> for the defaults.</param>
    /// <returns>The chart as a printable string.</returns>
    /// <exception cref="CchartsException">
    /// The series were empty or of differing lengths, held NaN/infinity, or the
    /// dimensions were out of range.
    /// </exception>
    public static string StackedBar(
        IReadOnlyList<(string Name, IReadOnlyList<double> Values)> series,
        int width, int height, StackOptions? options = null)
    {
        ArgumentNullException.ThrowIfNull(series);
        NativeLibraryResolver.EnsureInstalled();

        options ??= new StackOptions();
        if (series.Count == 0)
        {
            throw new CchartsException(CchartsStatus.InvalidArgument,
                "need at least one series");
        }
        var cats = series[0].Values.Count;
        foreach (var s in series)
        {
            if (s.Values.Count != cats)
            {
                throw new CchartsException(CchartsStatus.InvalidArgument,
                    "all series must have the same number of values");
            }
        }

        var native = new NativeStackSeries[series.Count];
        var namePtrs = new IntPtr[series.Count];
        var valuesHandles = new GCHandle[series.Count];
        var colorPtrs = new List<IntPtr>();
        var background = IntPtr.Zero;
        GCHandle colorsHandle = default;
        GCHandle labelsHandle = default;
        try
        {
            for (var i = 0; i < series.Count; i++)
            {
                namePtrs[i] = Marshal.StringToCoTaskMemUTF8(series[i].Name);
                var values = series[i].Values.ToArray();
                valuesHandles[i] = GCHandle.Alloc(values, GCHandleType.Pinned);
                native[i] = new NativeStackSeries
                {
                    Name = namePtrs[i],
                    Values = valuesHandles[i].AddrOfPinnedObject(),
                };
            }

            // An empty C string means "emit no escape at all", which is
            // different from a null pointer (use the default color).
            background = options.Plain ? Empty() : Utf8(options.BackgroundColor);

            IntPtr colorsPtr = IntPtr.Zero;
            var plainColors = options.Plain;
            if (plainColors || options.Colors is { Count: > 0 })
            {
                var count = plainColors ? series.Count : options.Colors!.Count;
                var colorArr = new IntPtr[count + 1];
                for (var i = 0; i < count; i++)
                {
                    IntPtr ptr;
                    if (plainColors)
                    {
                        ptr = Empty();
                    }
                    else
                    {
                        var c = options.Colors![i];
                        ptr = string.IsNullOrEmpty(c) ? IntPtr.Zero : Utf8(c);
                    }
                    colorArr[i] = ptr;
                    if (ptr != IntPtr.Zero)
                    {
                        colorPtrs.Add(ptr);
                    }
                }
                colorArr[count] = IntPtr.Zero;
                colorsHandle = GCHandle.Alloc(colorArr, GCHandleType.Pinned);
                colorsPtr = colorsHandle.AddrOfPinnedObject();
            }

            IntPtr labelsPtr = IntPtr.Zero;
            if (options.CategoryLabels is { Length: > 0 })
            {
                var n = options.CategoryLabels.Length;
                var labelArr = new IntPtr[n + 1];
                for (var i = 0; i < n; i++)
                {
                    labelArr[i] = Marshal.StringToCoTaskMemUTF8(options.CategoryLabels[i] ?? "");
                }
                labelArr[n] = IntPtr.Zero;
                labelsHandle = GCHandle.Alloc(labelArr, GCHandleType.Pinned);
                labelsPtr = labelsHandle.AddrOfPinnedObject();
            }

            var settings = new NativeStackSettings
            {
                Colors = colorsPtr,
                BackgroundColor = background,
                CatLabels = labelsPtr,
                Series = options.Series ?? series.Count,
                Cats = options.Cats ?? cats,
                ShowLabels = options.ShowLabels ? 1 : 0,
                ShowPrices = options.ShowPrices ? 1 : 0,
            };

            var status = NativeMethods.Stack(native, series.Count, width, height,
                in settings, out var chart, out var length);
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
            if (colorsHandle.IsAllocated)
            {
                colorsHandle.Free();
            }
            if (labelsHandle.IsAllocated)
            {
                labelsHandle.Free();
            }
            foreach (var handle in valuesHandles)
            {
                if (handle.IsAllocated)
                {
                    handle.Free();
                }
            }
            if (background != IntPtr.Zero)
            {
                Marshal.FreeCoTaskMem(background);
            }
            foreach (var colorPtr in colorPtrs)
            {
                Marshal.FreeCoTaskMem(colorPtr);
            }
            foreach (var namePtr in namePtrs)
            {
                if (namePtr != IntPtr.Zero)
                {
                    Marshal.FreeCoTaskMem(namePtr);
                }
            }
        }
    }

    /// <summary>
    /// Renders a heatmap of a <c>rows</c> x <c>cols</c> row-major values matrix
    /// into a <paramref name="width"/> x <paramref name="height"/> grid. Every
    /// row must share the same length. Matrix elements map to the fixed
    /// deterministic colormap ladder by their position between the matrix
    /// min/max. A heatmap has no OHLC dataset, so this is a static method
    /// taking the matrix directly.
    /// </summary>
    /// <exception cref="CchartsException">on null/ragged input, non-finite
    /// values or bad dimensions.</exception>
    public static string Heatmap(IReadOnlyList<IReadOnlyList<double>> values,
        int width, int height, HeatmapOptions? options = null)
    {
        ArgumentNullException.ThrowIfNull(values);
        NativeLibraryResolver.EnsureInstalled();

        options ??= new HeatmapOptions();
        if (values.Count == 0)
        {
            throw new CchartsException(CchartsStatus.InvalidArgument,
                "need at least one matrix row");
        }
        var cols = values[0].Count;
        if (cols == 0)
        {
            throw new CchartsException(CchartsStatus.InvalidArgument,
                "matrix columns must not be empty");
        }
        foreach (var row in values)
        {
            if (row.Count != cols)
            {
                throw new CchartsException(CchartsStatus.InvalidArgument,
                    "all rows must have the same number of values");
            }
        }

        // Flatten the matrix row-major into a contiguous buffer.
        var flat = new double[values.Count * cols];
        var index = 0;
        foreach (var row in values)
        {
            foreach (var v in row) flat[index++] = v;
        }

        var colorPtrs = new List<IntPtr>();
        var labelStrPtrs = new List<IntPtr>();
        GCHandle rowHandle = default;
        GCHandle colHandle = default;
        try
        {
            // An empty C string means "emit no escape at all": `plain` blanks
            // the WHOLE colormap ladder (the heatmap's plain convention), so
            // every color becomes Empty(); otherwise a null/empty color is the
            // library default (IntPtr.Zero).
            IntPtr ColorFor(string? color)
            {
                var ptr = options.Plain ? Empty() : Utf8(color);
                if (ptr != IntPtr.Zero) colorPtrs.Add(ptr);
                return ptr;
            }

            IntPtr LabelArray(string[]? labels, ref GCHandle handle)
            {
                if (labels is not { Length: > 0 }) return IntPtr.Zero;
                var n = labels.Length;
                var arr = new IntPtr[n + 1];
                for (var i = 0; i < n; i++)
                {
                    arr[i] = Marshal.StringToCoTaskMemUTF8(labels[i] ?? "");
                    labelStrPtrs.Add(arr[i]);
                }
                arr[n] = IntPtr.Zero;
                handle = GCHandle.Alloc(arr, GCHandleType.Pinned);
                return handle.AddrOfPinnedObject();
            }

            var settings = new NativeHeatSettings
            {
                LowColor = ColorFor(options.LowColor),
                HighColor = ColorFor(options.HighColor),
                MidColor = ColorFor(options.MidColor),
                BackgroundColor = ColorFor(options.BackgroundColor),
                RowLabels = LabelArray(options.RowLabels, ref rowHandle),
                ColLabels = LabelArray(options.ColLabels, ref colHandle),
                ShowLabels = options.ShowLabels ? 1 : 0,
            };

            var status = NativeMethods.Heat(flat, values.Count, cols,
                width, height, in settings, out var chart, out var length);
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
            if (rowHandle.IsAllocated) rowHandle.Free();
            if (colHandle.IsAllocated) colHandle.Free();
            foreach (var ptr in labelStrPtrs)
            {
                if (ptr != IntPtr.Zero) Marshal.FreeCoTaskMem(ptr);
            }
            foreach (var ptr in colorPtrs)
            {
                if (ptr != IntPtr.Zero) Marshal.FreeCoTaskMem(ptr);
            }
        }
    }

    /// <summary>
    /// Renders a box plot of the given categories into a <paramref name="width"/>
    /// x <paramref name="height"/> grid. Each category carries its own (possibly
    /// ragged) samples array; the C core computes a nearest-rank five-number
    /// summary per category and draws each box and its whiskers over the global
    /// min/max span, so the binding passes raw samples and settings only. A box
    /// plot has no OHLC dataset, so this is a static method taking the
    /// categories directly.
    /// </summary>
    /// <param name="series">The categories: a name and that category's samples.</param>
    /// <param name="width">Chart width in cells.</param>
    /// <param name="height">Chart height in cells.</param>
    /// <param name="options">Box options, or <c>null</c> for the defaults.</param>
    /// <returns>The chart as a printable string.</returns>
    /// <exception cref="CchartsException">
    /// The categories were empty or held no samples, a sample was NaN/infinity,
    /// or the dimensions were out of range.
    /// </exception>
    public static string Boxplot(IReadOnlyList<BoxCategory> series,
        int width, int height, BoxOptions? options = null)
    {
        ArgumentNullException.ThrowIfNull(series);
        NativeLibraryResolver.EnsureInstalled();

        options ??= new BoxOptions();
        if (series.Count == 0)
        {
            throw new CchartsException(CchartsStatus.InvalidArgument,
                "need at least one category");
        }
        foreach (var cat in series)
        {
            if (cat.Samples.Count == 0)
            {
                throw new CchartsException(CchartsStatus.InvalidArgument,
                    "every category must have at least one sample");
            }
        }

        var native = new NativeBoxCategory[series.Count];
        var namePtrs = new IntPtr[series.Count];
        var samplesHandles = new GCHandle[series.Count];
        var colorPtrs = new List<IntPtr>();
        try
        {
            for (var i = 0; i < series.Count; i++)
            {
                namePtrs[i] = series[i].Name is null
                    ? IntPtr.Zero
                    : Marshal.StringToCoTaskMemUTF8(series[i].Name);
                var samples = series[i].Samples.ToArray();
                samplesHandles[i] = GCHandle.Alloc(samples, GCHandleType.Pinned);
                native[i] = new NativeBoxCategory
                {
                    Name = namePtrs[i],
                    Samples = samplesHandles[i].AddrOfPinnedObject(),
                    N = samples.Length,
                };
            }

            // An empty C string means "emit no escape at all", which is
            // different from a null pointer (use the default color).
            IntPtr ColorFor(string? color)
            {
                var ptr = options.Plain ? Empty() : Utf8(color);
                if (ptr != IntPtr.Zero) colorPtrs.Add(ptr);
                return ptr;
            }

            var settings = new NativeBoxSettings
            {
                RiseColor = ColorFor(options.RiseColor),
                AreaColor = ColorFor(options.AreaColor),
                BackgroundColor = ColorFor(options.BackgroundColor),
                ShowPrices = options.ShowPrices ? 1 : 0,
            };

            var status = NativeMethods.Box(native, series.Count, width, height,
                in settings, out var chart, out var length);
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
            foreach (var handle in samplesHandles)
            {
                if (handle.IsAllocated)
                {
                    handle.Free();
                }
            }
            foreach (var namePtr in namePtrs)
            {
                if (namePtr != IntPtr.Zero)
                {
                    Marshal.FreeCoTaskMem(namePtr);
                }
            }
            foreach (var ptr in colorPtrs)
            {
                if (ptr != IntPtr.Zero) Marshal.FreeCoTaskMem(ptr);
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
