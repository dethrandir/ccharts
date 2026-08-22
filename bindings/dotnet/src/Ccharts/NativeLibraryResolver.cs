using System.Reflection;
using System.Runtime.InteropServices;

namespace Ccharts;

/// <summary>
/// Finds the native library when it did not arrive through the NuGet package.
///
/// A published package carries runtimes/{rid}/native/, which the default
/// resolver handles on its own. This fallback exists for working inside the
/// repository, where the library comes from the CMake build directory, and for
/// deployments that place it somewhere unusual — set CCHARTS_NATIVE_DIR and it
/// will be found.
/// </summary>
internal static class NativeLibraryResolver
{
    private static int _installed;

    internal static void EnsureInstalled()
    {
        if (Interlocked.Exchange(ref _installed, 1) == 0)
        {
            NativeLibrary.SetDllImportResolver(typeof(NativeMethods).Assembly, Resolve);
        }
    }

    private static IntPtr Resolve(string name, Assembly assembly, DllImportSearchPath? path)
    {
        if (name != NativeMethods.Library)
        {
            return IntPtr.Zero;
        }

        // Let the runtime try first: a NuGet-installed package resolves here.
        if (NativeLibrary.TryLoad(name, assembly, path, out var handle))
        {
            return handle;
        }

        foreach (var candidate in CandidatePaths())
        {
            if (File.Exists(candidate) && NativeLibrary.TryLoad(candidate, out handle))
            {
                return handle;
            }
        }

        return IntPtr.Zero;
    }

    private static IEnumerable<string> CandidatePaths()
    {
        var fileName = OperatingSystem.IsWindows() ? "ccharts_abi.dll"
            : OperatingSystem.IsMacOS() ? "libccharts_abi.dylib"
            : "libccharts_abi.so";

        var explicitDir = Environment.GetEnvironmentVariable("CCHARTS_NATIVE_DIR");
        if (!string.IsNullOrEmpty(explicitDir))
        {
            yield return Path.Combine(explicitDir, fileName);
        }

        // Walk up from the assembly looking for the repository's CMake output.
        var dir = Path.GetDirectoryName(AppContext.BaseDirectory);
        for (var i = 0; i < 8 && dir is not null; i++)
        {
            yield return Path.Combine(dir, "build", fileName);
            yield return Path.Combine(dir, "build", "Release", fileName);
            dir = Path.GetDirectoryName(dir);
        }
    }
}
