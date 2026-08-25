using System.Runtime.InteropServices;

namespace Ccharts;

/// <summary>
/// The sixteen ANSI colors and the reset sequence, read from the native
/// library so every binding uses the same values. Any escape string works
/// wherever these are accepted, so 256-color and truecolor do too.
/// </summary>
public static class Color
{
    private static string At(int index)
    {
        NativeLibraryResolver.EnsureInstalled();
        return Marshal.PtrToStringUTF8(NativeMethods.ColorAt(index)) ?? string.Empty;
    }

    /// <summary>Black.</summary>
    public static string Black { get; } = At(0);
    /// <summary>Red.</summary>
    public static string Red { get; } = At(1);
    /// <summary>Green.</summary>
    public static string Green { get; } = At(2);
    /// <summary>Yellow.</summary>
    public static string Yellow { get; } = At(3);
    /// <summary>Blue.</summary>
    public static string Blue { get; } = At(4);
    /// <summary>Magenta.</summary>
    public static string Magenta { get; } = At(5);
    /// <summary>Cyan.</summary>
    public static string Cyan { get; } = At(6);
    /// <summary>White.</summary>
    public static string White { get; } = At(7);
    /// <summary>Bright black (gray).</summary>
    public static string BrightBlack { get; } = At(8);
    /// <summary>Bright red.</summary>
    public static string BrightRed { get; } = At(9);
    /// <summary>Bright green.</summary>
    public static string BrightGreen { get; } = At(10);
    /// <summary>Bright yellow.</summary>
    public static string BrightYellow { get; } = At(11);
    /// <summary>Bright blue.</summary>
    public static string BrightBlue { get; } = At(12);
    /// <summary>Bright magenta.</summary>
    public static string BrightMagenta { get; } = At(13);
    /// <summary>Bright cyan.</summary>
    public static string BrightCyan { get; } = At(14);
    /// <summary>Bright white.</summary>
    public static string BrightWhite { get; } = At(15);
    /// <summary>Reset.</summary>
    public static string Reset { get; } = At(16);
}

/// <summary>
/// How a chart is drawn. The default instance takes every library default:
/// green rising, red falling, no background, no area fill, no labels.
/// </summary>
public sealed record ChartOptions
{
    /// <summary>Chart width in cells. Default 60.</summary>
    public int Width { get; init; } = 60;

    /// <summary>Chart height in cells. Default 8.</summary>
    public int Height { get; init; } = 8;

    /// <summary>ANSI escape for rising values and candles. Default green.</summary>
    public string? RiseColor { get; init; }

    /// <summary>ANSI escape for falling values and candles. Default red.</summary>
    public string? FallColor { get; init; }

    /// <summary>ANSI escape filling empty cells. Default: the terminal background.</summary>
    public string? BackgroundColor { get; init; }

    /// <summary>ANSI escape filling the area below a line chart. Default: nothing.</summary>
    public string? AreaColor { get; init; }

    /// <summary>
    /// Draw the whole chart in one color chosen from the overall change,
    /// instead of coloring each segment by its own direction.
    /// </summary>
    public bool SingleColor { get; init; }

    /// <summary>Print max/min price labels in a left margin.</summary>
    public bool ShowPrices { get; init; }

    /// <summary>Print the first and last timestamp under the chart.</summary>
    public bool ShowTimes { get; init; }

    /// <summary>
    /// Render with no ANSI escapes at all, overriding every color. Use it when
    /// the chart is going somewhere that does not interpret escapes — a log
    /// file, an HTML block, a commit message.
    /// </summary>
    public bool Plain { get; init; }
}

/// <summary>One slice of a pie chart: a legend label and a positive amount.</summary>
/// <param name="Label">
/// Drawn in the legend when <see cref="PieOptions.ShowLegend"/> is set; may be
/// <c>null</c> to omit it.
/// </param>
/// <param name="Value">
/// A positive amount; the pie computes the percentage from the sum. A value
/// <c>&lt;= 0</c> (zero, negative, NaN, inf) makes the whole render return the
/// empty string rather than an error.
/// </param>
public sealed record PieSlice(string? Label, double Value);

/// <summary>
/// How a pie chart is drawn. The default instance gives a filled disk, no
/// override colors, a legend without percentages.
/// </summary>
public sealed record PieOptions
{
    /// <summary>Chart width in cells. Default 24.</summary>
    public int Width { get; init; } = 24;

    /// <summary>Chart height in cells. Default 10.</summary>
    public int Height { get; init; } = 10;

    /// <summary>Hollow out the center (a donut) instead of a filled disk.</summary>
    public bool Donut { get; init; }

    /// <summary>
    /// Per-slice ANSI escapes; slice <c>i</c> uses
    /// <c>Colors[i % Colors.Count]</c>. A <c>null</c> entry means the default
    /// palette color for that index; <c>null</c> for the whole list selects
    /// the fixed default palette.
    /// </summary>
    public IReadOnlyList<string?>? Colors { get; init; }

    /// <summary>Print one <c>label  value (pct%)</c> line per slice below the disk.</summary>
    public bool ShowLegend { get; init; } = true;

    /// <summary>Append <c>(NN%)</c> to each legend entry.</summary>
    public bool ShowPct { get; init; }

    /// <summary>Angular gap between slices, in radians. Default <c>0</c> (adjacent).</summary>
    public double SliceGap { get; init; }

    /// <summary>
    /// Donut thickness in <c>[0, 1]</c>: <c>0</c> = a filled disk, <c>1</c> = a
    /// hairline ring. A negative value (the default <c>-1</c>) leaves it to
    /// <see cref="Donut"/> (<c>0.5</c> for a donut, <c>0</c> for a disk). Values
    /// above 1 are clamped.
    /// </summary>
    public double InnerRadiusRatio { get; init; } = -1;

    /// <summary>
    /// Legend entry format: <c>0</c> = <c>label  value</c> (+ <c>(NN%)</c> with
    /// <see cref="ShowPct"/>), <c>1</c> = <c>label  NN%</c>, <c>2</c> =
    /// <c>value  (NN%)</c>, <c>3</c> = <c>label</c> only. Unknown values fall
    /// back to <c>0</c>.
    /// </summary>
    public int LegendFormat { get; init; }

    /// <summary>
    /// Angle (radians) at which slice 0 begins. A negative value (the default
    /// <c>-1</c>) uses the library default (12 o'clock).
    /// </summary>
    public double StartAngle { get; init; } = -1;

    /// <summary>Sweep the slices clockwise instead of the default counter-clockwise.</summary>
    public bool CounterClockwise { get; init; }

    /// <summary>Text drawn in the hollow center of a donut (only when there is a hollow).</summary>
    public string? CenterText { get; init; }
}

/// <summary>
/// How a histogram is drawn. The default instance gives green bars, an auto
/// bin count, and an auto value window from the data.
/// </summary>
public sealed record HistogramOptions
{
    /// <summary>Chart width in cells. Default 60.</summary>
    public int Width { get; init; } = 60;

    /// <summary>Chart height in cells. Default 8.</summary>
    public int Height { get; init; } = 8;

    /// <summary>ANSI escape for the bars. Default green.</summary>
    public string? Color { get; init; }

    /// <summary>ANSI escape filling empty cells. Default: the terminal background.</summary>
    public string? BackgroundColor { get; init; }

    /// <summary>
    /// Number of equal-width bins; <c>0</c> selects a value from the sample
    /// count, bounded by the chart width. Default <c>0</c> (auto).
    /// </summary>
    public int BinCount { get; init; }

    /// <summary>
    /// Lower edge of the value window; <c>null</c> (NaN) uses the data
    /// minimum. When set, must be strictly less than <see cref="MaxValue"/>.
    /// </summary>
    public double? MinValue { get; init; }

    /// <summary>
    /// Upper edge of the value window; <c>null</c> (NaN) uses the data
    /// maximum. When set, must be strictly greater than <see cref="MinValue"/>.
    /// </summary>
    public double? MaxValue { get; init; }

    /// <summary>Print a value-axis footer row (window min left, max right).</summary>
    public bool ShowBins { get; init; }

    /// <summary>Print the max-count / min-count labels in a left margin.</summary>
    public bool ShowPrices { get; init; }

    /// <summary>
    /// Render with no ANSI escapes at all, overriding every color. Use it when
    /// the chart is going somewhere that does not interpret escapes — a log
    /// file, an HTML block, a commit message.
    /// </summary>
    public bool Plain { get; init; }
}

/// <summary>
/// How a sparkline is drawn. The default instance gives a one-row-high line
/// that follows the data, with no margins and the default rising color.
/// </summary>
public sealed record SparklineOptions
{
    /// <summary>Chart width in cells. Default 24.</summary>
    public int Width { get; init; } = 24;

    /// <summary>Chart height in cells. Default 1.</summary>
    public int Height { get; init; } = 1;

    /// <summary>ANSI escape for the rising line. Default green.</summary>
    public string? RiseColor { get; init; }

    /// <summary>ANSI escape filling the area below the line. Default: nothing.</summary>
    public string? AreaColor { get; init; }

    /// <summary>Sub-pixels reserved above the line so it does not clip. Default 0.</summary>
    public int MinAbove { get; init; }

    /// <summary>Sub-pixels reserved below the line so it does not clip. Default 0.</summary>
    public int MinBelow { get; init; }

    /// <summary>
    /// Render with no ANSI escapes at all, overriding every color. Use it when
    /// the chart is going somewhere that does not interpret escapes — a log
    /// file, an HTML block, a commit message.
    /// </summary>
    public bool Plain { get; init; }
}

/// <summary>
/// How a bar chart is drawn. The default instance gives green bars, no
/// background, no label or value footer.
/// </summary>
public sealed record BarOptions
{
    /// <summary>ANSI escape for the bars. Default green.</summary>
    public string? Color { get; init; }

    /// <summary>ANSI escape filling empty cells. Default: the terminal background.</summary>
    public string? BackgroundColor { get; init; }

    /// <summary>
    /// Print each column's label in a footer row below the chart.
    /// </summary>
    public bool ShowLabels { get; init; }

    /// <summary>
    /// Print the max bar value and 0 (the baseline) in a left value-axis margin.
    /// </summary>
    public bool ShowPrices { get; init; }

    /// <summary>
    /// Render with no ANSI escapes at all, overriding every color. Use it when
    /// the chart is going somewhere that does not interpret escapes — a log
    /// file, an HTML block, a commit message.
    /// </summary>
    public bool Plain { get; init; }
}
