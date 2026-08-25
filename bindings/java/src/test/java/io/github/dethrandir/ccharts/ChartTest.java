package io.github.dethrandir.ccharts;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertNotEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.util.List;
import java.util.concurrent.CompletableFuture;
import java.util.stream.IntStream;
import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.CsvSource;
import org.junit.jupiter.params.provider.ValueSource;

class ChartTest {

    private static final double[] OPEN = {328.75, 330.0, 317.25, 320.0, 306.0};
    private static final double[] HIGH = {330.0, 330.25, 321.0, 328.75, 307.25};
    private static final double[] LOW = {323.75, 317.5, 314.5, 317.75, 300.75};
    private static final double[] CLOSE = {328.0, 317.5, 321.0, 318.0, 301.0};
    private static final long[] TS =
            {1784505600L, 1784592000L, 1784678400L, 1784764800L, 1784851200L};

    private static Chart sample() {
        return Chart.fromArrays(OPEN, HIGH, LOW, CLOSE, TS);
    }

    @Test
    @DisplayName("renders both chart types")
    void rendersBothChartTypes() {
        try (Chart chart = sample()) {
            assertEquals(5, chart.length());

            String line = chart.line(ChartOptions.builder().size(40, 4).build());
            assertEquals(4, line.stripTrailing().split("\n").length);

            String candle = chart.candle(ChartOptions.builder().size(40, 4).build());
            assertTrue(candle.contains("│"), "candles draw wicks");
        }
    }

    @Test
    @DisplayName("the JSON and array entry points agree")
    void jsonAndArraysAgree() {
        String document = """
                [{"ts":"2026-07-20T00:00:00+00:00","open":328.75,"high":330.0,"low":323.75,"close":328.0},
                 {"ts":"2026-07-21T00:00:00+00:00","open":330.0,"high":330.25,"low":317.5,"close":317.5},
                 {"ts":"2026-07-22T00:00:00+00:00","open":317.25,"high":321.0,"low":314.5,"close":321.0},
                 {"ts":"2026-07-23T00:00:00+00:00","open":320.0,"high":328.75,"low":317.75,"close":318.0},
                 {"ts":"2026-07-24T00:00:00+00:00","open":306.0,"high":307.25,"low":300.75,"close":301.0}]
                """;
        ChartOptions options = ChartOptions.builder()
                .showPrices(true).showTimes(true).build();

        try (Chart fromJson = Chart.fromJson(document); Chart fromArrays = sample()) {
            assertEquals(fromArrays.line(options), fromJson.line(options));
            assertEquals(fromArrays.candle(options), fromJson.candle(options));
        }
    }

    @Test
    @DisplayName("CSV skips blank lines instead of zero-filling candles")
    void csvSkipsBlankLines() {
        try (Chart chart = Chart.fromCsv("1,2,0.5,1.5\n\n   \n2,3,1,2.5\n")) {
            assertEquals(2, chart.length());
        }
    }

    @Test
    @DisplayName("timestamps are optional")
    void timestampsAreOptional() {
        try (Chart chart = Chart.fromArrays(OPEN, HIGH, LOW, CLOSE)) {
            String out = chart.line(ChartOptions.builder()
                    .size(20, 3).showTimes(true).build());
            assertEquals(3, out.stripTrailing().split("\n").length);
            assertFalse(out.contains("2026"));
        }
    }

    @Test
    @DisplayName("colors come from the native library")
    void colorsComeFromTheNativeLibrary() {
        assertEquals("\u001b[34m", Color.BLUE.escape());
        assertEquals("\u001b[97m", Color.BRIGHT_WHITE.escape());

        try (Chart chart = sample()) {
            String out = chart.line(ChartOptions.builder()
                    .size(40, 4).rise(Color.BLUE).fall(Color.RED).build());
            assertTrue(out.contains("\u001b[34m") && out.contains("\u001b[31m"));
        }
    }

    @Test
    @DisplayName("custom escapes pass through")
    void customEscapesPassThrough() {
        try (Chart chart = sample()) {
            String out = chart.line(ChartOptions.builder()
                    .riseAnsi("\u001b[38;5;208m").build());
            assertTrue(out.contains("\u001b[38;5;208m"));
        }
    }

    @ParameterizedTest
    @CsvSource({"0,5", "5,0", "-1,5", "1000,2000"})
    @DisplayName("bad dimensions are an error, not an empty string")
    void badDimensionsAreAnError(int width, int height) {
        try (Chart chart = sample()) {
            CchartsException error = assertThrows(CchartsException.class,
                    () -> chart.line(ChartOptions.builder().size(width, height).build()));
            assertEquals(CchartsException.Status.DIMENSIONS, error.status());
        }
    }

    @Test
    @DisplayName("empty and mismatched columns are rejected")
    void invalidColumnsAreRejected() {
        assertEquals(CchartsException.Status.INVALID_ARGUMENT,
                assertThrows(CchartsException.class,
                        () -> Chart.fromArrays(new double[0], new double[0],
                                new double[0], new double[0])).status());
        assertEquals(CchartsException.Status.INVALID_ARGUMENT,
                assertThrows(CchartsException.class,
                        () -> Chart.fromArrays(new double[] {1, 2}, new double[] {2},
                                new double[] {0, 1}, new double[] {1, 2})).status());
    }

    @ParameterizedTest
    @ValueSource(doubles = {Double.NaN, Double.POSITIVE_INFINITY, Double.NEGATIVE_INFINITY})
    @DisplayName("non-finite prices are rejected")
    void nonFinitePricesAreRejected(double bad) {
        CchartsException error = assertThrows(CchartsException.class,
                () -> Chart.fromArrays(new double[] {bad}, new double[] {2},
                        new double[] {0}, new double[] {1}));
        assertEquals(CchartsException.Status.NON_FINITE, error.status());
    }

    @ParameterizedTest
    @ValueSource(strings = {"not json", "[]", "{\"open\": 1}"})
    @DisplayName("malformed JSON is rejected")
    void malformedJsonIsRejected(String document) {
        CchartsException error = assertThrows(CchartsException.class,
                () -> Chart.fromJson(document));
        assertEquals(CchartsException.Status.PARSE, error.status());
    }

    @Test
    @DisplayName("close is idempotent and guards later renders")
    void closeIsIdempotent() {
        Chart chart = sample();
        chart.close();
        chart.close();
        assertThrows(IllegalStateException.class, chart::line);
    }

    @Test
    @DisplayName("charts render identically from several threads")
    void concurrentRendering() {
        try (Chart chart = sample()) {
            ChartOptions options = ChartOptions.builder().size(30, 4).build();
            String expected = chart.line(options);
            CompletableFuture<?>[] futures = IntStream.range(0, 8)
                    .mapToObj(i -> CompletableFuture.runAsync(
                            () -> assertEquals(expected, chart.line(options))))
                    .toArray(CompletableFuture[]::new);
            CompletableFuture.allOf(futures).join();
        }
    }

    @Test
    @DisplayName("exposes library metadata")
    void exposesLibraryMetadata() {
        assertEquals("0.2.2", Chart.version());
        assertEquals(100000, Chart.maxDim());
        assertEquals(1000000, Chart.maxCells());
    }

    @Test
    @DisplayName("pie renders a disk, a donut and the empty string for zero values")
    void pieRendersDiskDonutAndEmptyForZeroValues() {
        List<PieSlice> slices = List.of(
                new PieSlice("Alpha", 40),
                new PieSlice("Beta", 30),
                new PieSlice("Gamma", 30));

        String disk = Chart.pie(slices, PieOptions.builder()
                .showLegend(true)
                .showPct(true)
                .build());
        assertTrue(disk.contains("Alpha  40 (40%)"));

        String donut = Chart.pie(slices, PieOptions.builder()
                .donut(true)
                .showLegend(true)
                .build());
        assertNotEquals(disk, donut);

        assertEquals("", Chart.pie(List.of(new PieSlice("Zero", 0)),
                PieOptions.builder().showLegend(true).build()));

        CchartsException error = assertThrows(CchartsException.class,
                () -> Chart.pie(List.of(new PieSlice("Bad", Double.NaN))));
        assertEquals(CchartsException.Status.NON_FINITE, error.status());

        assertThrows(CchartsException.class, () -> Chart.pie(List.of()));
    }

    @Test
    @DisplayName("histogram renders bins, prices, range and rejects empty/NaN samples")
    void histogramRendersAndValidates() {
        double[] samples = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

        String bars = Chart.histogram(samples, 20, 6, HistogramOptions.builder()
                .binCount(6)
                .build());
        assertTrue(bars.contains("▄") || bars.contains("█"),
                "histogram draws bars");

        String color = Chart.histogram(samples, 20, 6, HistogramOptions.builder()
                .color(Color.BLUE)
                .build());
        assertTrue(color.contains("\u001b[34m"), "bar color passes through");

        String prices = Chart.histogram(samples, 40, 6, HistogramOptions.builder()
                .showBins(true)
                .showPrices(true)
                .build());
        assertTrue(prices.contains("12") || prices.contains("1"),
                "prices/bins show labels");

        String bounded = Chart.histogram(samples, 40, 6, HistogramOptions.builder()
                .minValue(0.0)
                .maxValue(20.0)
                .build());
        assertTrue(bounded.length() > 0);

        CchartsException empty = assertThrows(CchartsException.class,
                () -> Chart.histogram(new double[0], 40, 6));
        assertEquals(CchartsException.Status.INVALID_ARGUMENT, empty.status());

        CchartsException bad = assertThrows(CchartsException.class,
                () -> Chart.histogram(new double[] {Double.NaN, 1.0}, 40, 6));
        assertEquals(CchartsException.Status.NON_FINITE, bad.status());
    }

    @Test
    @DisplayName("sparkline draws a trend line, passes color/margins and rejects empty/NaN samples")
    void sparklineRendersAndValidates() {
        double[] samples = {5, 7, 4, 8, 6, 9, 4, 7, 10, 8, 12, 6};

        String line = Chart.sparkline(samples, 24, 1);
        assertTrue(line.length() > 0, "sparkline draws a row");

        String colored = Chart.sparkline(samples, 24, 2, SparklineOptions.builder()
                .rise(Color.BLUE)
                .area(Color.BRIGHT_BLACK)
                .minAbove(2)
                .minBelow(1)
                .build());
        assertTrue(colored.contains("\u001b[34m"), "rise color passes through");
        assertTrue(colored.contains("\u001b[90m"), "area color passes through");

        CchartsException empty = assertThrows(CchartsException.class,
                () -> Chart.sparkline(new double[0], 24, 1));
        assertEquals(CchartsException.Status.INVALID_ARGUMENT, empty.status());

        CchartsException bad = assertThrows(CchartsException.class,
                () -> Chart.sparkline(new double[] {Double.NaN, 1.0}, 24, 1));
        assertEquals(CchartsException.Status.NON_FINITE, bad.status());
    }

    @Test
    @DisplayName("boxplot draws boxes, passes colors/margins and rejects empty/NaN samples")
    void boxplotRendersAndValidates() {
        String[] names = {"A", "B"};
        double[][] samples = {
            {1, 4, 2, 5, 3},
            {1, 2, 3, 4, 5, 6, 7, 8, 9},
        };

        String base = Chart.boxplot(names, samples, 10, 8);
        assertTrue(base.contains("\u2588"), "box glyphs present");
        assertEquals(8, base.strip().split("\n").length);

        String priced = Chart.boxplot(names, samples, 10, 8, BoxplotOptions.builder()
                .showPrices(true)
                .build());
        assertNotEquals(base, priced, "show_prices must take effect");

        String colored = Chart.boxplot(names, samples, 10, 8, BoxplotOptions.builder()
                .riseColor(Color.BLUE)
                .areaColor(Color.BRIGHT_BLACK)
                .build());
        assertNotEquals(base, colored, "rise/area colors must take effect");
        assertTrue(colored.contains("\u001b[34m"), "rise color passes through");

        String plain = Chart.boxplot(names, samples, 10, 8, BoxplotOptions.builder()
                .plain(true)
                .build());
        assertFalse(plain.contains("\u001b"), "plain must override every color");

        CchartsException empty = assertThrows(CchartsException.class,
                () -> Chart.boxplot(new String[] {"A"}, new double[][] {new double[0]}, 10, 8));
        assertEquals(CchartsException.Status.INVALID_ARGUMENT, empty.status());

        CchartsException bad = assertThrows(CchartsException.class,
                () -> Chart.boxplot(new String[] {"A", "B"},
                        new double[][] {{1.0}, {Double.NaN}}, 10, 8));
        assertEquals(CchartsException.Status.NON_FINITE, bad.status());
    }
}
