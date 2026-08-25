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
     * Renders a sparkline of the given scalar samples. A sparkline has no
     * OHLC dataset, so this is a static method taking the raw sample values.
     *
     * @param samples the close-like trend values to draw
     * @param width chart width in cells
     * @param height chart height in cells
     * @param options sparkline options
     * @return the chart as a printable string
     * @throws CchartsException if the samples are empty, hold NaN/infinity,
     *     or the dimensions are out of range
     */
    public static String sparkline(double[] samples, int width, int height,
            SparklineOptions options) {
        if (samples == null || samples.length == 0) {
            throw new CchartsException(CchartsException.Status.INVALID_ARGUMENT,
                    "need at least one sample");
        }

        try (Arena arena = Arena.ofConfined()) {
            MemorySegment settings = arena.allocate(Native.SPARK_SETTINGS);
            // An empty C string means "emit no escape at all", which is
            // different from a null pointer (use the default color).
            String forced = options.plain() ? "" : null;
            settings.set(ValueLayout.ADDRESS, 0,
                    text(arena, forced != null ? forced : options.riseColor()));
            settings.set(ValueLayout.ADDRESS, 8,
                    text(arena, forced != null ? forced : options.areaColor()));
            settings.set(ValueLayout.JAVA_INT, 16, options.minAbove());
            settings.set(ValueLayout.JAVA_INT, 20, options.minBelow());

            MemorySegment out = arena.allocate(ValueLayout.ADDRESS);
            MemorySegment lengthOut = arena.allocate(ValueLayout.JAVA_LONG);
            int status = (int) Native.SPARK.invokeExact(
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
     * Renders a sparkline of the given samples with the default options.
     *
     * @param samples the close-like trend values to draw
     * @param width chart width in cells
     * @param height chart height in cells
     * @return the chart as a printable string
     */
    public static String sparkline(double[] samples, int width, int height) {
        return sparkline(samples, width, height, SparklineOptions.DEFAULTS);
    }

    /**
     * Renders a bar chart of the given named values. A bar chart has no OHLC
     * dataset, so this is a static method taking the labels and values
     * directly.
     *
     * @param labels bar column labels, one per value
     * @param values the bar heights; negatives are clamped to zero
     * @param width chart width in cells
     * @param height chart height in cells
     * @param options bar options
     * @return the chart as a printable string
     * @throws CchartsException if the arrays are empty, of differing lengths,
     *     hold NaN/infinity, or the dimensions are out of range
     */
    public static String bar(String[] labels, double[] values, int width, int height,
            BarOptions options) {
        if (labels == null || labels.length == 0) {
            throw new CchartsException(CchartsException.Status.INVALID_ARGUMENT,
                    "need at least one bar");
        }
        if (values == null || values.length != labels.length) {
            throw new CchartsException(CchartsException.Status.INVALID_ARGUMENT,
                    "labels and values must have the same length");
        }

        try (Arena arena = Arena.ofConfined()) {
            // Each bar is a {label, value} cell: a C-string segment and a
            // double. The label is null-safe (the renderer treats a missing
            // label as empty).
            MemorySegment items = arena.allocate(Native.BAR_ITEM, labels.length);
            for (int i = 0; i < labels.length; i++) {
                long offset = i * Native.BAR_ITEM.byteSize();
                items.set(ValueLayout.ADDRESS, offset, text(arena, labels[i]));
                items.set(ValueLayout.JAVA_DOUBLE, offset + 8, values[i]);
            }

            MemorySegment settings = arena.allocate(Native.BAR_SETTINGS);
            // An empty C string means "emit no escape at all", which is
            // different from a null pointer (use the default color).
            String forced = options.plain() ? "" : null;
            settings.set(ValueLayout.ADDRESS, 0,
                    text(arena, forced != null ? forced : options.riseColor()));
            settings.set(ValueLayout.ADDRESS, 8,
                    text(arena, forced != null ? forced : options.backgroundColor()));
            settings.set(ValueLayout.JAVA_INT, 16, options.showLabels() ? 1 : 0);
            settings.set(ValueLayout.JAVA_INT, 20, options.showPrices() ? 1 : 0);

            MemorySegment out = arena.allocate(ValueLayout.ADDRESS);
            MemorySegment lengthOut = arena.allocate(ValueLayout.JAVA_LONG);
            int status = (int) Native.BAR.invokeExact(
                    items, labels.length, width, height, settings, out, lengthOut);
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
     * Renders a bar chart of the given named values with the default options.
     *
     * @param labels bar column labels, one per value
     * @param values the bar heights; negatives are clamped to zero
     * @param width chart width in cells
     * @param height chart height in cells
     * @return the chart as a printable string
     */
    public static String bar(String[] labels, double[] values, int width, int height) {
        return bar(labels, values, width, height, BarOptions.DEFAULTS);
    }

    /**
     * Renders a stacked bar chart of the given series. Each series contributes
     * one vertical segment per category, and a category's bar height is the SUM
     * of its series' values. A stacked bar chart has no OHLC dataset, so this
     * is a static method taking the series directly.
     *
     * @param names the series names, one per series
     * @param values a 2-D matrix: one {@code double[]} row per series, each of
     *     the same length (one entry per category)
     * @param width chart width in cells
     * @param height chart height in cells
     * @param options stack options
     * @return the chart as a printable string
     * @throws CchartsException if the series are empty or of differing
     *     lengths, hold NaN/infinity, or the dimensions are out of range
     */
    public static String stackedBar(String[] names, double[][] values, int width, int height,
            StackOptions options) {
        if (names == null || names.length == 0) {
            throw new CchartsException(CchartsException.Status.INVALID_ARGUMENT,
                    "need at least one series");
        }
        if (values == null || values.length != names.length) {
            throw new CchartsException(CchartsException.Status.INVALID_ARGUMENT,
                    "names and values must have the same length");
        }
        int cats = values[0].length;
        for (double[] row : values) {
            if (row == null || row.length != cats) {
                throw new CchartsException(CchartsException.Status.INVALID_ARGUMENT,
                        "all series must have the same number of values");
            }
        }

        try (Arena arena = Arena.ofConfined()) {
            // Each series is a {name, values} cell: a C-string segment and a
            // pointer to its double[] row.
            MemorySegment seriesSegment = arena.allocate(Native.STACK_SERIES, names.length);
            for (int i = 0; i < names.length; i++) {
                long offset = i * Native.STACK_SERIES.byteSize();
                seriesSegment.set(ValueLayout.ADDRESS, offset, text(arena, names[i]));
                seriesSegment.set(ValueLayout.ADDRESS, offset + 8,
                        arena.allocateFrom(ValueLayout.JAVA_DOUBLE, values[i]));
            }

            // colors is a NULL-terminated per-series palette override. `plain`
            // forces every entry to the empty C string (emit no escape);
            // otherwise a null array selects the fixed default palette.
            String[] colorEntries = options.plain() ? new String[names.length] : options.colors();
            MemorySegment colorsSegment = MemorySegment.NULL;
            if (colorEntries != null) {
                colorsSegment = arena.allocate(ValueLayout.ADDRESS, colorEntries.length + 1);
                for (int i = 0; i < colorEntries.length; i++) {
                    String c = colorEntries[i];
                    colorsSegment.set(ValueLayout.ADDRESS, i * 8,
                            options.plain() ? text(arena, "")
                                    : (c == null ? MemorySegment.NULL : text(arena, c)));
                }
                colorsSegment.set(ValueLayout.ADDRESS,
                        (long) colorEntries.length * 8, MemorySegment.NULL);
            }

            MemorySegment labelsSegment = MemorySegment.NULL;
            String[] labels = options.categoryLabels();
            if (labels != null) {
                labelsSegment = arena.allocate(ValueLayout.ADDRESS, labels.length + 1);
                for (int i = 0; i < labels.length; i++) {
                    labelsSegment.set(ValueLayout.ADDRESS, i * 8,
                            text(arena, labels[i] == null ? "" : labels[i]));
                }
                labelsSegment.set(ValueLayout.ADDRESS,
                        (long) labels.length * 8, MemorySegment.NULL);
            }

            MemorySegment settings = arena.allocate(Native.STACK_SETTINGS);
            // An empty C string means "emit no escape at all", which is
            // different from a null pointer (use the default color).
            settings.set(ValueLayout.ADDRESS, 0, colorsSegment);
            settings.set(ValueLayout.ADDRESS, 8, text(arena,
                    options.plain() ? "" : options.backgroundColor()));
            settings.set(ValueLayout.ADDRESS, 16, labelsSegment);
            settings.set(ValueLayout.JAVA_INT, 24, options.series() > 0 ? options.series() : names.length);
            settings.set(ValueLayout.JAVA_INT, 28, options.cats() > 0 ? options.cats() : cats);
            settings.set(ValueLayout.JAVA_INT, 32, options.showLabels() ? 1 : 0);
            settings.set(ValueLayout.JAVA_INT, 36, options.showPrices() ? 1 : 0);

            MemorySegment out = arena.allocate(ValueLayout.ADDRESS);
            MemorySegment lengthOut = arena.allocate(ValueLayout.JAVA_LONG);
            int status = (int) Native.STACK.invokeExact(
                    seriesSegment, names.length, width, height, settings, out, lengthOut);
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
     * Renders a stacked bar chart of the given series with the default options.
     *
     * @param names the series names, one per series
     * @param values a 2-D matrix: one {@code double[]} row per series, each of
     *     the same length (one entry per category)
     * @param width chart width in cells
     * @param height chart height in cells
     * @return the chart as a printable string
     */
    public static String stackedBar(String[] names, double[][] values, int width, int height) {
        return stackedBar(names, values, width, height, StackOptions.DEFAULTS);
    }

    /**
     * Renders a heatmap of a {@code rows} x {@code cols} row-major {@code values}
     * matrix into a {@code width} x {@code height} grid. Every row must share
     * the same length. Matrix elements map to the fixed deterministic colormap
     * ladder by their position between the matrix min/max. A heatmap has no
     * OHLC dataset, so this is a static method taking the matrix directly.
     *
     * @param values  the 2-D matrix, one {@code double[]} row per {@code rows}, each
     *                of the same length (one entry per {@code cols})
     * @param width   chart width in cells
     * @param height  chart height in cells
     * @param options rendering options
     * @return the chart as a printable string
     */
    public static String heatmap(double[][] values, int width, int height,
            HeatmapOptions options) {
        if (values == null || values.length == 0) {
            throw new CchartsException(CchartsException.Status.INVALID_ARGUMENT,
                    "need at least one matrix row");
        }
        int rows = values.length;
        int cols = values[0].length;
        if (cols == 0) {
            throw new CchartsException(CchartsException.Status.INVALID_ARGUMENT,
                    "matrix columns must not be empty");
        }
        for (double[] row : values) {
            if (row == null || row.length != cols) {
                throw new CchartsException(CchartsException.Status.INVALID_ARGUMENT,
                        "all rows must have the same number of values");
            }
        }

        try (Arena arena = Arena.ofConfined()) {
            // Flatten the matrix row-major into a contiguous double[] segment.
            double[] flat = new double[rows * cols];
            int idx = 0;
            for (double[] row : values) {
                for (double v : row) {
                    flat[idx++] = v;
                }
            }
            MemorySegment valuesSegment = arena.allocateFrom(ValueLayout.JAVA_DOUBLE, flat);

            MemorySegment settings = arena.allocate(Native.HEAT_SETTINGS);
            // An empty C string means "emit no escape at all": the heatmap's
            // plain convention is that an empty low_color blanks the WHOLE
            // ladder, so plain forces "" over every color (vs. null = library
            // default when not plain).
            settings.set(ValueLayout.ADDRESS, 0, text(arena,
                    options.plain() ? "" : options.lowColor()));
            settings.set(ValueLayout.ADDRESS, 8, text(arena,
                    options.plain() ? "" : options.highColor()));
            settings.set(ValueLayout.ADDRESS, 16, text(arena,
                    options.plain() ? "" : options.midColor()));
            settings.set(ValueLayout.ADDRESS, 24, text(arena,
                    options.plain() ? "" : options.backgroundColor()));
            settings.set(ValueLayout.ADDRESS, 32, labelSegment(arena, options.rowLabels()));
            settings.set(ValueLayout.ADDRESS, 40, labelSegment(arena, options.colLabels()));
            settings.set(ValueLayout.JAVA_INT, 48, options.showLabels() ? 1 : 0);

            MemorySegment out = arena.allocate(ValueLayout.ADDRESS);
            MemorySegment lengthOut = arena.allocate(ValueLayout.JAVA_LONG);
            int status = (int) Native.HEAT.invokeExact(
                    valuesSegment, rows, cols, width, height, settings, out, lengthOut);
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
     * Renders a heatmap of the given matrix with the default options.
     *
     * @param values the 2-D matrix, one {@code double[]} row per {@code rows}
     * @param width  chart width in cells
     * @param height chart height in cells
     * @return the chart as a printable string
     */
    public static String heatmap(double[][] values, int width, int height) {
        return heatmap(values, width, height, HeatmapOptions.DEFAULTS);
    }

    /**
     * Renders a box plot of the given categories into a {@code width} x
     * {@code height} grid. Each category carries its own (possibly ragged)
     * samples array; the C core computes a nearest-rank five-number summary
     * per category and draws each box and its whiskers over the global
     * min/max span, so the binding passes raw samples and settings only. A
     * box plot has no OHLC dataset, so this is a static method taking the
     * categories directly.
     *
     * @param names   the category names, one per category (the core does not
     *                print them; kept for the binding API)
     * @param samples a 2-D matrix: one {@code double[]} row per category,
     *                each with that category's samples (rows may be ragged)
     * @param width   chart width in cells
     * @param height  chart height in cells
     * @param options rendering options
     * @return the chart as a printable string
     * @throws CchartsException if the categories are empty or hold no
     *     samples, a sample is NaN/infinity, or the dimensions are out of
     *     range
     */
    public static String boxplot(String[] names, double[][] samples, int width, int height,
            BoxplotOptions options) {
        if (names == null || names.length == 0) {
            throw new CchartsException(CchartsException.Status.INVALID_ARGUMENT,
                    "need at least one category");
        }
        if (samples == null || samples.length != names.length) {
            throw new CchartsException(CchartsException.Status.INVALID_ARGUMENT,
                    "names and samples must have the same length");
        }
        for (double[] row : samples) {
            if (row == null || row.length == 0) {
                throw new CchartsException(CchartsException.Status.INVALID_ARGUMENT,
                        "every category must have at least one sample");
            }
        }

        try (Arena arena = Arena.ofConfined()) {
            // Each category is a {name, samples, n} cell: a C-string segment
            // and a pointer to its double[] samples row.
            MemorySegment categories = arena.allocate(Native.BOX_CATEGORY, names.length);
            for (int i = 0; i < names.length; i++) {
                long offset = i * Native.BOX_CATEGORY.byteSize();
                categories.set(ValueLayout.ADDRESS, offset, text(arena, names[i]));
                categories.set(ValueLayout.ADDRESS, offset + 8,
                        arena.allocateFrom(ValueLayout.JAVA_DOUBLE, samples[i]));
                categories.set(ValueLayout.JAVA_INT, offset + 16, samples[i].length);
            }

            MemorySegment settings = arena.allocate(Native.BOX_SETTINGS);
            // An empty C string means "emit no escape at all", which is
            // different from a null pointer (use the default color).
            settings.set(ValueLayout.ADDRESS, 0, text(arena,
                    options.plain() ? "" : options.riseColor()));
            settings.set(ValueLayout.ADDRESS, 8, text(arena,
                    options.plain() ? "" : options.areaColor()));
            settings.set(ValueLayout.ADDRESS, 16, text(arena,
                    options.plain() ? "" : options.backgroundColor()));
            settings.set(ValueLayout.JAVA_INT, 24, options.showPrices() ? 1 : 0);

            MemorySegment out = arena.allocate(ValueLayout.ADDRESS);
            MemorySegment lengthOut = arena.allocate(ValueLayout.JAVA_LONG);
            int status = (int) Native.BOX.invokeExact(
                    categories, names.length, width, height, settings, out, lengthOut);
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
     * Renders a box plot of the given categories with the default options.
     *
     * @param names   the category names, one per category
     * @param samples a 2-D matrix: one {@code double[]} row per category,
     *                each with that category's samples (rows may be ragged)
     * @param width   chart width in cells
     * @param height  chart height in cells
     * @return the chart as a printable string
     */
    public static String boxplot(String[] names, double[][] samples, int width, int height) {
        return boxplot(names, samples, width, height, BoxplotOptions.DEFAULTS);
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

    /**
     * Allocates a C-string pointer array for a label list, or NULL when absent.
     * The renderer indexes it by row/column count, so a NULL-terminator is
     * defensive; labels are plain text, so {@code plain} does not affect them.
     */
    private static MemorySegment labelSegment(Arena arena, String[] labels) {
        if (labels == null || labels.length == 0) {
            return MemorySegment.NULL;
        }
        MemorySegment segment = arena.allocate(ValueLayout.ADDRESS, labels.length + 1);
        for (int i = 0; i < labels.length; i++) {
            segment.set(ValueLayout.ADDRESS, i * 8L,
                    text(arena, labels[i] == null ? "" : labels[i]));
        }
        segment.set(ValueLayout.ADDRESS, (long) labels.length * 8, MemorySegment.NULL);
        return segment;
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
