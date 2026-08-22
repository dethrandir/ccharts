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
}
