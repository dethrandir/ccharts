package io.github.dethrandir.ccharts;

import java.io.IOException;
import java.io.InputStream;
import java.lang.foreign.AddressLayout;
import java.lang.foreign.Arena;
import java.lang.foreign.FunctionDescriptor;
import java.lang.foreign.Linker;
import java.lang.foreign.MemoryLayout;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.StructLayout;
import java.lang.foreign.SymbolLookup;
import java.lang.foreign.ValueLayout;
import java.lang.invoke.MethodHandle;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.Locale;

/**
 * The FFM plumbing for the ccharts C ABI: locating the native library and
 * binding its entry points. No JNI code is involved — this is plain Java on
 * top of {@code java.lang.foreign}, which is why the binding needs JDK 22 or
 * newer.
 *
 * <p>Package-private; {@link Chart} is the supported surface.
 */
final class Native {

    private Native() {
    }

    /** Layout of {@code ccharts_settings}: four pointers then three ints. */
    static final StructLayout SETTINGS = MemoryLayout.structLayout(
            ValueLayout.ADDRESS.withName("rise_color"),
            ValueLayout.ADDRESS.withName("fall_color"),
            ValueLayout.ADDRESS.withName("bg_color"),
            ValueLayout.ADDRESS.withName("area_color"),
            ValueLayout.JAVA_INT.withName("single_color"),
            ValueLayout.JAVA_INT.withName("show_prices"),
            ValueLayout.JAVA_INT.withName("show_times"),
            MemoryLayout.paddingLayout(4));

    /** Layout of {@code ccharts_pie_slice}: a label pointer then a double. */
    static final StructLayout PIE_SLICE = MemoryLayout.structLayout(
            ValueLayout.ADDRESS.withName("label"),
            ValueLayout.JAVA_DOUBLE.withName("value"));

    /** Layout of {@code ccharts_hist_settings}: two pointers, an int, padding,
     * then two doubles, then two ints. */
    static final StructLayout HIST_SETTINGS = MemoryLayout.structLayout(
            ValueLayout.ADDRESS.withName("rise_color"),
            ValueLayout.ADDRESS.withName("bg_color"),
            ValueLayout.JAVA_INT.withName("bin_count"),
            MemoryLayout.paddingLayout(4),
            ValueLayout.JAVA_DOUBLE.withName("min_value"),
            ValueLayout.JAVA_DOUBLE.withName("max_value"),
            ValueLayout.JAVA_INT.withName("show_bins"),
            ValueLayout.JAVA_INT.withName("show_prices"));

    private static final Linker LINKER = Linker.nativeLinker();

    /** Lives as long as the process: the library must not be unloaded. */
    private static final Arena LIBRARY_ARENA = Arena.ofShared();

    private static final SymbolLookup LOOKUP = loadLibrary();

    private static final ValueLayout.OfInt INT = ValueLayout.JAVA_INT;
    private static final AddressLayout PTR = ValueLayout.ADDRESS;

    static final MethodHandle FROM_ARRAYS = downcall("ccharts_from_arrays",
            FunctionDescriptor.of(INT, PTR, PTR, PTR,
                    PTR, PTR, INT, PTR));
    static final MethodHandle PARSE_JSON = downcall("ccharts_parse_json",
            FunctionDescriptor.of(INT, PTR, PTR));
    static final MethodHandle PARSE_CSV = downcall("ccharts_parse_csv",
            FunctionDescriptor.of(INT, PTR, ValueLayout.JAVA_BYTE,
                    ValueLayout.JAVA_BYTE, PTR));
    static final MethodHandle DATA_LEN = downcall("ccharts_data_len",
            FunctionDescriptor.of(INT, PTR));
    static final MethodHandle DATA_FREE = downcall("ccharts_data_free",
            FunctionDescriptor.ofVoid(PTR));
    static final MethodHandle LINE = downcall("ccharts_line",
            FunctionDescriptor.of(INT, PTR, INT, INT, PTR,
                    PTR, PTR));
    static final MethodHandle CANDLE = downcall("ccharts_candle",
            FunctionDescriptor.of(INT, PTR, INT, INT, PTR,
                    PTR, PTR));
    static final MethodHandle STRING_FREE = downcall("ccharts_string_free",
            FunctionDescriptor.ofVoid(PTR));
    static final MethodHandle PIE_FROM_SLICES = downcall("ccharts_pie_from_slices",
            FunctionDescriptor.of(INT, PTR, INT, INT, INT, INT,
                    PTR, INT, INT, INT, ValueLayout.JAVA_DOUBLE,
                    ValueLayout.JAVA_DOUBLE, INT, ValueLayout.JAVA_DOUBLE,
                    INT, PTR, PTR, PTR));
    static final MethodHandle HIST = downcall("ccharts_hist",
            FunctionDescriptor.of(INT, PTR, INT, INT, INT, PTR,
                    PTR, PTR));
    static final MethodHandle COLOR = downcall("ccharts_color",
            FunctionDescriptor.of(PTR, INT));
    static final MethodHandle ERROR_MESSAGE = downcall("ccharts_error_message",
            FunctionDescriptor.of(PTR, INT));
    static final MethodHandle VERSION = downcall("ccharts_version",
            FunctionDescriptor.of(PTR));
    static final MethodHandle MAX_DIM = downcall("ccharts_max_dim",
            FunctionDescriptor.of(INT));
    static final MethodHandle MAX_CELLS = downcall("ccharts_max_cells",
            FunctionDescriptor.of(INT));

    private static MethodHandle downcall(String name, FunctionDescriptor descriptor) {
        MemorySegment symbol = LOOKUP.find(name).orElseThrow(
                () -> new UnsatisfiedLinkError("ccharts: missing symbol " + name));
        return LINKER.downcallHandle(symbol, descriptor);
    }

    /**
     * Finds the native library: the copy packaged in this jar first, then
     * {@code CCHARTS_NATIVE_DIR}, then the repository's CMake output (which is
     * how the tests run before anything is packaged).
     */
    private static SymbolLookup loadLibrary() {
        String fileName = libraryFileName();

        Path packaged = extractFromJar(fileName);
        if (packaged != null) {
            return SymbolLookup.libraryLookup(packaged, LIBRARY_ARENA);
        }

        String explicit = System.getenv("CCHARTS_NATIVE_DIR");
        if (explicit != null && !explicit.isEmpty()) {
            Path candidate = Path.of(explicit, fileName);
            if (Files.exists(candidate)) {
                return SymbolLookup.libraryLookup(candidate, LIBRARY_ARENA);
            }
        }

        Path dir = Path.of("").toAbsolutePath();
        for (int i = 0; i < 8 && dir != null; i++) {
            for (String sub : new String[] {"build", "build/Release"}) {
                Path candidate = dir.resolve(sub).resolve(fileName);
                if (Files.exists(candidate)) {
                    return SymbolLookup.libraryLookup(candidate, LIBRARY_ARENA);
                }
            }
            dir = dir.getParent();
        }

        throw new UnsatisfiedLinkError(
                "ccharts: could not find " + fileName + ". Build it with "
                + "`cmake -S . -B build && cmake --build build` or set "
                + "CCHARTS_NATIVE_DIR.");
    }

    private static String libraryFileName() {
        String os = System.getProperty("os.name", "").toLowerCase(Locale.ROOT);
        if (os.contains("win")) {
            return "ccharts_abi.dll";
        }
        if (os.contains("mac") || os.contains("darwin")) {
            return "libccharts_abi.dylib";
        }
        return "libccharts_abi.so";
    }

    /** Copies the packaged library to a temp file, or returns null if absent. */
    private static Path extractFromJar(String fileName) {
        String resource = "/native/" + platformDirectory() + "/" + fileName;
        try (InputStream in = Native.class.getResourceAsStream(resource)) {
            if (in == null) {
                return null;
            }
            Path temp = Files.createTempFile("ccharts", fileName);
            temp.toFile().deleteOnExit();
            Files.copy(in, temp, StandardCopyOption.REPLACE_EXISTING);
            return temp;
        } catch (IOException e) {
            throw new UnsatisfiedLinkError(
                    "ccharts: could not unpack " + resource + ": " + e.getMessage());
        }
    }

    private static String platformDirectory() {
        String os = System.getProperty("os.name", "").toLowerCase(Locale.ROOT);
        String arch = System.getProperty("os.arch", "").toLowerCase(Locale.ROOT);

        String osName = os.contains("win") ? "windows"
                : (os.contains("mac") || os.contains("darwin")) ? "macos"
                : "linux";
        String archName = switch (arch) {
            case "amd64", "x86_64" -> "x86_64";
            case "aarch64", "arm64" -> "aarch64";
            default -> arch;
        };
        return osName + "-" + archName;
    }

    /** Reads a NUL-terminated UTF-8 string the library owns. */
    static String readString(MemorySegment pointer) {
        if (pointer == null || pointer.equals(MemorySegment.NULL)) {
            return "";
        }
        return pointer.reinterpret(Long.MAX_VALUE).getString(0);
    }

    static String colorAt(int index) {
        try {
            return readString((MemorySegment) COLOR.invokeExact(index));
        } catch (Throwable t) {
            throw wrap(t);
        }
    }

    static String errorMessage(int status) {
        try {
            return readString((MemorySegment) ERROR_MESSAGE.invokeExact(status));
        } catch (Throwable t) {
            throw wrap(t);
        }
    }

    static RuntimeException wrap(Throwable t) {
        if (t instanceof RuntimeException runtime) {
            return runtime;
        }
        if (t instanceof Error error) {
            throw error;
        }
        return new IllegalStateException("ccharts: native call failed", t);
    }
}
