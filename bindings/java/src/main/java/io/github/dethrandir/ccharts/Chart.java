package io.github.dethrandir.ccharts;

import java.lang.foreign.Arena;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.ValueLayout;
import java.lang.ref.Cleaner;
import java.nio.charset.StandardCharsets;
import java.util.List;

/**
 * A parsed OHLC dataset that can be rendered as a line or candlestick chart.
 *
 * <p>Charts are immutable once built and the native library holds no mutable
 * global state, so an instance is safe to render from several threads.
 *
 * <pre>{@code
 * try (Chart chart = Chart.fromArrays(open, high, low, close, epochSeconds)) {
 *     System.out.print(chart.line(ChartOptions.builder()
 *             .size(60, 8)
 *             .rise(Color.BLUE)
 *             .showPrices(true)
 *             .build()));
 * }
 * }</pre>
 *
 * <p>{@link #close()} releases the native memory; a {@link Cleaner} covers a
 * forgotten call, but relying on it keeps the memory alive longer.
 *
 * <p>Requires JDK 22 or newer (the FFM API). On JDK 24 and later, add
 * {@code --enable-native-access=ALL-UNNAMED} to silence the native-access
 * warning.
 */
public final class Chart implements AutoCloseable {

    private static final Cleaner CLEANER = Cleaner.create();

    /** Frees the dataset without capturing the Chart itself. */
    private record Release(MemorySegment handle) implements Runnable {
        @Override
        public void run() {
            try {
                Native.DATA_FREE.invokeExact(handle);
            } catch (Throwable t) {
                throw Native.wrap(t);
            }
        }
    }

    private final MemorySegment handle;
    private final int length;
    private final Cleaner.Cleanable cleanable;
    private volatile boolean closed;

    private Chart(MemorySegment handle) {
        this.handle = handle;
        this.cleanable = CLEANER.register(this, new Release(handle));
        try {
            this.length = (int) Native.DATA_LEN.invokeExact(handle);
        } catch (Throwable t) {
            throw Native.wrap(t);
        }
    }

    /**
     * Builds a chart from four equal-length price columns.
     *
     * @param open opening prices
     * @param high high prices
     * @param low low prices
     * @param close closing prices
     * @param ts epoch seconds, or {@code null} when the candles have no
     *     timestamps
     * @return the chart
     * @throws CchartsException if the columns are empty, of differing lengths,
     *     or hold NaN or infinity
     */
    public static Chart fromArrays(double[] open, double[] high, double[] low,
            double[] close, long[] ts) {
        int n = open.length;
        if (n == 0) {
            throw new CchartsException(CchartsException.Status.INVALID_ARGUMENT,
                    "need at least one candle");
        }
        if (high.length != n || low.length != n || close.length != n) {
            throw new CchartsException(CchartsException.Status.INVALID_ARGUMENT,
                    "open, high, low and close must have the same length");
        }
        if (ts != null && ts.length != n) {
            throw new CchartsException(CchartsException.Status.INVALID_ARGUMENT,
                    "ts must have the same length as the price columns");
        }

        try (Arena arena = Arena.ofConfined()) {
            MemorySegment out = arena.allocate(ValueLayout.ADDRESS);
            // invokeExact matches the argument's *static* type, and a
            // conditional expression there would be typed as Object, so the
            // optional timestamp column goes through a typed local.
            MemorySegment epochs = ts == null ? MemorySegment.NULL
                    : arena.allocateFrom(ValueLayout.JAVA_LONG, ts);
            int status = (int) Native.FROM_ARRAYS.invokeExact(
                    arena.allocateFrom(ValueLayout.JAVA_DOUBLE, open),
                    arena.allocateFrom(ValueLayout.JAVA_DOUBLE, high),
                    arena.allocateFrom(ValueLayout.JAVA_DOUBLE, low),
                    arena.allocateFrom(ValueLayout.JAVA_DOUBLE, close),
                    epochs, n, out);
            CchartsException.throwIfError(status);
            return new Chart(out.get(ValueLayout.ADDRESS, 0));
        } catch (Throwable t) {
            throw Native.wrap(t);
        }
    }

    /**
     * Builds a chart from four price columns without timestamps.
     *
     * @param open opening prices
     * @param high high prices
     * @param low low prices
     * @param close closing prices
     * @return the chart
     */
    public static Chart fromArrays(double[] open, double[] high, double[] low,
            double[] close) {
        return fromArrays(open, high, low, close, null);
    }

    /**
     * Builds a chart from the fixed-schema JSON document: an array of objects
     * with {@code ts}, {@code open}, {@code high}, {@code low} and
     * {@code close}.
     *
     * @param json the document
     * @return the chart
     * @throws CchartsException if the document cannot be parsed
     */
    public static Chart fromJson(String json) {
        try (Arena arena = Arena.ofConfined()) {
            MemorySegment out = arena.allocate(ValueLayout.ADDRESS);
            int status = (int) Native.PARSE_JSON.invokeExact(
                    arena.allocateFrom(json), out);
            CchartsException.throwIfError(status);
            return new Chart(out.get(ValueLayout.ADDRESS, 0));
        } catch (Throwable t) {
            throw Native.wrap(t);
        }
    }

    /**
     * Builds a chart from CSV rows of
     * {@code open,high,low,close[,timestamp]}. Blank lines are skipped.
     *
     * @param csv the document
     * @param valueSeparator field separator, typically {@code ','}
     * @param lineSeparator row separator, typically {@code '\n'}
     * @return the chart
     * @throws CchartsException if the document cannot be parsed
     */
    public static Chart fromCsv(String csv, char valueSeparator, char lineSeparator) {
        if (valueSeparator == 0 || lineSeparator == 0
                || valueSeparator > 127 || lineSeparator > 127) {
            throw new CchartsException(CchartsException.Status.INVALID_ARGUMENT,
                    "separators must be non-NUL ASCII characters");
        }
        try (Arena arena = Arena.ofConfined()) {
            MemorySegment out = arena.allocate(ValueLayout.ADDRESS);
            int status = (int) Native.PARSE_CSV.invokeExact(
                    arena.allocateFrom(csv), (byte) valueSeparator,
                    (byte) lineSeparator, out);
            CchartsException.throwIfError(status);
            return new Chart(out.get(ValueLayout.ADDRESS, 0));
        } catch (Throwable t) {
            throw Native.wrap(t);
        }
    }

    /**
     * Builds a chart from comma-separated CSV rows.
     *
     * @param csv the document
     * @return the chart
     */
    public static Chart fromCsv(String csv) {
        return fromCsv(csv, ',', '\n');
    }

    /**
     * Renders a pie/donut chart from the given slices. A pie has no OHLC
     * dataset, so this is a static method taking the slices directly.
     *
     * @param slices the slices, in the order they are drawn
     * @param options pie options
     * @return the chart as a printable string
     * @throws CchartsException if the slices are empty, hold NaN/infinity, or
     *     the dimensions are out of range
     */
    public static String pie(List<PieSlice> slices, PieOptions options) {
        if (slices == null || slices.isEmpty()) {
            throw new CchartsException(CchartsException.Status.INVALID_ARGUMENT,
                    "need at least one slice");
        }

        try (Arena arena = Arena.ofConfined()) {
            MemorySegment slicesSeg = arena.allocate(Native.PIE_SLICE, slices.size());
            for (int i = 0; i < slices.size(); i++) {
                PieSlice slice = slices.get(i);
                slicesSeg.set(ValueLayout.ADDRESS, i * Native.PIE_SLICE.byteSize(),
                        text(arena, slice.label()));
                slicesSeg.set(ValueLayout.JAVA_DOUBLE, i * Native.PIE_SLICE.byteSize() + 8,
                        slice.value());
            }

            MemorySegment colorsSeg = MemorySegment.NULL;
            int colorCount = 0;
            if (options.colors() != null && options.colors().length > 0) {
                String[] colors = options.colors();
                colorsSeg = arena.allocate(ValueLayout.ADDRESS, colors.length);
                for (int i = 0; i < colors.length; i++) {
                    colorsSeg.set(ValueLayout.ADDRESS, i * ValueLayout.ADDRESS.byteSize(),
                            text(arena, colors[i]));
                }
                colorCount = colors.length;
            }

            MemorySegment out = arena.allocate(ValueLayout.ADDRESS);
            MemorySegment lengthOut = arena.allocate(ValueLayout.JAVA_LONG);
            // Nullable: a typed local keeps invokeExact's static-type matching
            // (a conditional expression here would be typed Object).
            MemorySegment centerText = text(arena, options.centerText());
            int status = (int) Native.PIE_FROM_SLICES.invokeExact(
                    slicesSeg, slices.size(), options.width(), options.height(),
                    options.donut() ? 1 : 0, colorsSeg, colorCount,
                    options.showLegend() ? 1 : 0, options.showPct() ? 1 : 0,
                    options.sliceGap(), options.innerRadiusRatio(),
                    options.legendFormat(), options.startAngle(),
                    options.counterClockwise() ? 1 : 0, centerText,
                    out, lengthOut);
            CchartsException.throwIfError(status);

            MemorySegment chart = out.get(ValueLayout.ADDRESS, 0);
            long size = lengthOut.get(ValueLayout.JAVA_LONG, 0);
            try {
                byte[] bytes = chart.reinterpret(size).toArray(ValueLayout.JAVA_BYTE);
                return new String(bytes, StandardCharsets.UTF_8);
            } finally {
                Native.STRING_FREE.invokeExact(chart);
            }
        } catch (Throwable t) {
            throw Native.wrap(t);
        }
    }

    /**
     * Renders a pie/donut chart with the default options.
     *
     * @param slices the slices, in the order they are drawn
     * @return the chart as a printable string
     */
    public static String pie(List<PieSlice> slices) {
        return pie(slices, PieOptions.DEFAULTS);
    }

    /**
     * Renders a histogram of the given scalar samples. A histogram has no
     * OHLC dataset, so this is a static method taking the raw sample values.
     *
     * @param samples the scalar values to bin
     * @param width chart width in cells
     * @param height chart height in cells
     * @param options histogram options
     * @return the chart as a printable string
     * @throws CchartsException if the samples are empty, hold NaN/infinity,
     *     or the dimensions are out of range
     */
    public static String histogram(double[] samples, int width, int height,
            HistogramOptions options) {
        if (samples == null || samples.length == 0) {
            throw new CchartsException(CchartsException.Status.INVALID_ARGUMENT,
                    "need at least one sample");
        }

        try (Arena arena = Arena.ofConfined()) {
            MemorySegment settings = arena.allocate(Native.HIST_SETTINGS);
            // An empty C string means "emit no escape at all", which is
            // different from a null pointer (use the default color).
            String forced = options.plain() ? "" : null;
            settings.set(ValueLayout.ADDRESS, 0,
                    text(arena, forced != null ? forced : options.color()));
            settings.set(ValueLayout.ADDRESS, 8,
                    text(arena, forced != null ? forced : options.backgroundColor()));
            settings.set(ValueLayout.JAVA_INT, 16, options.binCount());
            settings.set(ValueLayout.JAVA_DOUBLE, 24, options.minValue());
            settings.set(ValueLayout.JAVA_DOUBLE, 32, options.maxValue());
            settings.set(ValueLayout.JAVA_INT, 40, options.showBins() ? 1 : 0);
            settings.set(ValueLayout.JAVA_INT, 44, options.showPrices() ? 1 : 0);

            MemorySegment out = arena.allocate(ValueLayout.ADDRESS);
            MemorySegment lengthOut = arena.allocate(ValueLayout.JAVA_LONG);
            int status = (int) Native.HIST.invokeExact(
                    arena.allocateFrom(ValueLayout.JAVA_DOUBLE, samples),
                    samples.length, width, height, settings, out, lengthOut);
            CchartsException.throwIfError(status);

            MemorySegment chart = out.get(ValueLayout.ADDRESS, 0);
            long size = lengthOut.get(ValueLayout.JAVA_LONG, 0);
            try {
                byte[] bytes = chart.reinterpret(size).toArray(ValueLayout.JAVA_BYTE);
                return new String(bytes, StandardCharsets.UTF_8);
            } finally {
                Native.STRING_FREE.invokeExact(chart);
            }
        } catch (Throwable t) {
            throw Native.wrap(t);
        }
    }

    /**
     * Renders a histogram of the given samples with the default options.
     *
     * @param samples the scalar values to bin
     * @param width chart width in cells
     * @param height chart height in cells
     * @return the chart as a printable string
     */
    public static String histogram(double[] samples, int width, int height) {
        return histogram(samples, width, height, HistogramOptions.DEFAULTS);
    }

    /**
     * Number of candles in the dataset.
     *
     * @return the candle count
     */
    public int length() {
        return length;
    }

    /**
     * Renders a line chart of the closing prices.
     *
     * @param options rendering options
     * @return the chart as a printable string
     * @throws CchartsException if the dimensions are out of range
     */
    public String line(ChartOptions options) {
        return render(true, options);
    }

    /**
     * Renders a line chart with the default options.
     *
     * @return the chart as a printable string
     */
    public String line() {
        return render(true, ChartOptions.DEFAULTS);
    }

    /**
     * Renders a candlestick chart.
     *
     * @param options rendering options
     * @return the chart as a printable string
     * @throws CchartsException if the dimensions are out of range
     */
    public String candle(ChartOptions options) {
        return render(false, options);
    }

    /**
     * Renders a candlestick chart with the default options.
     *
     * @return the chart as a printable string
     */
    public String candle() {
        return render(false, ChartOptions.DEFAULTS);
    }

    private String render(boolean line, ChartOptions options) {
        if (closed) {
            throw new IllegalStateException("ccharts: chart is closed");
        }

        try (Arena arena = Arena.ofConfined()) {
            MemorySegment settings = arena.allocate(Native.SETTINGS);
            // An empty C string means "emit no escape at all", which is
            // different from a null pointer (use the default color).
            String forced = options.plain() ? "" : null;
            settings.set(ValueLayout.ADDRESS, 0,
                    text(arena, forced != null ? forced : options.riseColor()));
            settings.set(ValueLayout.ADDRESS, 8,
                    text(arena, forced != null ? forced : options.fallColor()));
            settings.set(ValueLayout.ADDRESS, 16,
                    text(arena, forced != null ? forced : options.backgroundColor()));
            settings.set(ValueLayout.ADDRESS, 24,
                    text(arena, forced != null ? forced : options.areaColor()));
            settings.set(ValueLayout.JAVA_INT, 32, options.singleColor() ? 1 : 0);
            settings.set(ValueLayout.JAVA_INT, 36, options.showPrices() ? 1 : 0);
            settings.set(ValueLayout.JAVA_INT, 40, options.showTimes() ? 1 : 0);

            MemorySegment out = arena.allocate(ValueLayout.ADDRESS);
            MemorySegment lengthOut = arena.allocate(ValueLayout.JAVA_LONG);

            int status = line
                    ? (int) Native.LINE.invokeExact(handle, options.width(),
                            options.height(), settings, out, lengthOut)
                    : (int) Native.CANDLE.invokeExact(handle, options.width(),
                            options.height(), settings, out, lengthOut);
            CchartsException.throwIfError(status);

            MemorySegment chart = out.get(ValueLayout.ADDRESS, 0);
            long size = lengthOut.get(ValueLayout.JAVA_LONG, 0);
            try {
                byte[] bytes = chart.reinterpret(size)
                        .toArray(ValueLayout.JAVA_BYTE);
                return new String(bytes, StandardCharsets.UTF_8);
            } finally {
                Native.STRING_FREE.invokeExact(chart);
            }
        } catch (Throwable t) {
            throw Native.wrap(t);
        }
    }

    /** null means "library default" (NULL); "" means "emit no escape". */
    private static MemorySegment text(Arena arena, String value) {
        return value == null ? MemorySegment.NULL : arena.allocateFrom(value);
    }

    /** Releases the native dataset. Safe to call more than once. */
    @Override
    public void close() {
        if (!closed) {
            closed = true;
            cleanable.clean();
        }
    }

    /**
     * Version of the underlying C library.
     *
     * @return the version string
     */
    public static String version() {
        try {
            return Native.readString((MemorySegment) Native.VERSION.invokeExact());
        } catch (Throwable t) {
            throw Native.wrap(t);
        }
    }

    /**
     * Largest width or height in cells ({@code CC_MAX_DIM}).
     *
     * @return the limit
     */
    public static int maxDim() {
        try {
            return (int) Native.MAX_DIM.invokeExact();
        } catch (Throwable t) {
            throw Native.wrap(t);
        }
    }

    /**
     * Largest number of cells in a chart ({@code CC_MAX_CELLS}).
     *
     * @return the limit
     */
    public static int maxCells() {
        try {
            return (int) Native.MAX_CELLS.invokeExact();
        } catch (Throwable t) {
            throw Native.wrap(t);
        }
    }
}
