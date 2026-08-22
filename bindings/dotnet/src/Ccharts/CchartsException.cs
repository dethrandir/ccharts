using System.Runtime.InteropServices;

namespace Ccharts;

/// <summary>Status codes reported by the ccharts C ABI.</summary>
public enum CchartsStatus
{
    /// <summary>No error.</summary>
    Ok = 0,
    /// <summary>Empty or mismatched input.</summary>
    InvalidArgument = 1,
    /// <summary>The JSON or CSV document could not be parsed.</summary>
    Parse = 2,
    /// <summary>An allocation failed.</summary>
    OutOfMemory = 3,
    /// <summary>A price was NaN or infinite.</summary>
    NonFinite = 4,
    /// <summary>Width or height was not positive, or exceeded the limits.</summary>
    Dimensions = 5,
}

/// <summary>An error raised by the chart library.</summary>
public sealed class CchartsException : Exception
{
    internal CchartsException(int status)
        : base(Marshal.PtrToStringUTF8(NativeMethods.ErrorMessage(status)) ?? "ccharts error")
    {
        Status = (CchartsStatus)status;
    }

    internal CchartsException(CchartsStatus status, string message) : base(message)
    {
        Status = status;
    }

    /// <summary>The status code behind this error.</summary>
    public CchartsStatus Status { get; }

    internal static void ThrowIfError(int status)
    {
        if (status != 0)
        {
            throw new CchartsException(status);
        }
    }
}
