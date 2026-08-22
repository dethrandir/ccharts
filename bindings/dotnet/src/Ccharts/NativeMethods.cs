using System.Runtime.InteropServices;

namespace Ccharts;

/// <summary>Layout of ccharts_settings from abi/ccharts_abi.h.</summary>
[StructLayout(LayoutKind.Sequential)]
internal struct NativeSettings
{
    public IntPtr RiseColor;
    public IntPtr FallColor;
    public IntPtr BackgroundColor;
    public IntPtr AreaColor;
    public int SingleColor;
    public int ShowPrices;
    public int ShowTimes;
}

/// <summary>
/// P/Invoke declarations for the ccharts C ABI. Strings are handled as raw
/// pointers on both directions: the library owns what it returns, and
/// <see cref="Chart"/> copies and releases immediately.
/// </summary>
internal static partial class NativeMethods
{
    internal const string Library = "ccharts_abi";

    [LibraryImport(Library, EntryPoint = "ccharts_from_arrays")]
    internal static partial int FromArrays(
        ReadOnlySpan<double> open, ReadOnlySpan<double> high,
        ReadOnlySpan<double> low, ReadOnlySpan<double> close,
        ReadOnlySpan<long> ts, int n, out IntPtr data);

    [LibraryImport(Library, EntryPoint = "ccharts_parse_json")]
    internal static partial int ParseJson(IntPtr json, out IntPtr data);

    [LibraryImport(Library, EntryPoint = "ccharts_parse_csv")]
    internal static partial int ParseCsv(IntPtr csv, byte valueSeparator,
        byte lineSeparator, out IntPtr data);

    [LibraryImport(Library, EntryPoint = "ccharts_data_len")]
    internal static partial int DataLength(IntPtr data);

    [LibraryImport(Library, EntryPoint = "ccharts_data_free")]
    internal static partial void DataFree(IntPtr data);

    [LibraryImport(Library, EntryPoint = "ccharts_line")]
    internal static partial int Line(IntPtr data, int width, int height,
        in NativeSettings settings, out IntPtr chart, out nuint length);

    [LibraryImport(Library, EntryPoint = "ccharts_candle")]
    internal static partial int Candle(IntPtr data, int width, int height,
        in NativeSettings settings, out IntPtr chart, out nuint length);

    [LibraryImport(Library, EntryPoint = "ccharts_string_free")]
    internal static partial void StringFree(IntPtr chart);

    [LibraryImport(Library, EntryPoint = "ccharts_color")]
    internal static partial IntPtr ColorAt(int index);

    [LibraryImport(Library, EntryPoint = "ccharts_error_message")]
    internal static partial IntPtr ErrorMessage(int status);

    [LibraryImport(Library, EntryPoint = "ccharts_version")]
    internal static partial IntPtr Version();

    [LibraryImport(Library, EntryPoint = "ccharts_max_dim")]
    internal static partial int MaxDim();

    [LibraryImport(Library, EntryPoint = "ccharts_max_cells")]
    internal static partial int MaxCells();
}
