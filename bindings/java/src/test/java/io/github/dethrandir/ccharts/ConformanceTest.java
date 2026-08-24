package io.github.dethrandir.ccharts;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertNotNull;
import static org.junit.jupiter.api.Assertions.assertTrue;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import org.junit.jupiter.api.Test;

/**
 * Renders the shared conformance cases and compares them byte for byte with
 * conformance/golden/*.txt — the contract every ccharts binding is held to.
 */
class ConformanceTest {

    private static final Map<String, Color> NAMED_COLORS = new LinkedHashMap<>();

    static {
        NAMED_COLORS.put("black", Color.BLACK);
        NAMED_COLORS.put("red", Color.RED);
        NAMED_COLORS.put("green", Color.GREEN);
        NAMED_COLORS.put("yellow", Color.YELLOW);
        NAMED_COLORS.put("blue", Color.BLUE);
        NAMED_COLORS.put("magenta", Color.MAGENTA);
        NAMED_COLORS.put("cyan", Color.CYAN);
        NAMED_COLORS.put("white", Color.WHITE);
        NAMED_COLORS.put("bright_black", Color.BRIGHT_BLACK);
        NAMED_COLORS.put("bright_red", Color.BRIGHT_RED);
        NAMED_COLORS.put("bright_green", Color.BRIGHT_GREEN);
        NAMED_COLORS.put("bright_yellow", Color.BRIGHT_YELLOW);
        NAMED_COLORS.put("bright_blue", Color.BRIGHT_BLUE);
        NAMED_COLORS.put("bright_magenta", Color.BRIGHT_MAGENTA);
        NAMED_COLORS.put("bright_cyan", Color.BRIGHT_CYAN);
        NAMED_COLORS.put("bright_white", Color.BRIGHT_WHITE);
        NAMED_COLORS.put("reset", Color.RESET);
    }

    /** Walks up from the working directory to the repository's suite. */
    private static Path findSuite() {
        Path dir = Path.of("").toAbsolutePath();
        for (int i = 0; i < 8 && dir != null; i++) {
            Path candidate = dir.resolve("conformance");
            if (Files.exists(candidate.resolve("cases.json"))) {
                return candidate;
            }
            dir = dir.getParent();
        }
        return null;
    }

    private static Color color(JsonNode settings, String key) {
        JsonNode value = settings.get(key);
        if (value == null || value.isNull()) {
            return null;
        }
        Color color = NAMED_COLORS.get(value.asText());
        assertNotNull(color, "unknown color in cases.json: " + value.asText());
        return color;
    }

    private static double[] column(JsonNode dataset, String key) {
        JsonNode array = dataset.get(key);
        double[] values = new double[array.size()];
        for (int i = 0; i < values.length; i++) {
            values[i] = array.get(i).asDouble();
        }
        return values;
    }

    private static long[] timestamps(JsonNode dataset) {
        JsonNode array = dataset.get("ts");
        if (array == null || array.isNull()) {
            return null;
        }
        long[] values = new long[array.size()];
        for (int i = 0; i < values.length; i++) {
            values[i] = array.get(i).asLong();
        }
        return values;
    }

    @Test
    void matchesTheSharedGoldens() throws IOException {
        Path suite = findSuite();
        assertNotNull(suite, "conformance/cases.json not found above the working directory");

        JsonNode document = new ObjectMapper().readTree(
                Files.readAllBytes(suite.resolve("cases.json")));
        JsonNode datasets = document.get("datasets");
        JsonNode cases = document.get("cases");
        assertTrue(cases.size() >= 10, "conformance suite looks truncated");

        for (JsonNode testCase : cases) {
            String name = testCase.get("name").asText();
            JsonNode settings = testCase.get("settings");

            String rendered;
            if (testCase.get("chart").asText().equals("pie")) {
                List<PieSlice> slices = new ArrayList<>();
                for (JsonNode node : testCase.get("slices")) {
                    JsonNode label = node.get("label");
                    slices.add(new PieSlice(
                            label == null || label.isNull() ? null : label.asText(),
                            node.get("value").asDouble()));
                }

                JsonNode colorsNode = settings.get("colors");
                String[] colors = null;
                if (colorsNode != null && colorsNode.isArray()) {
                    colors = new String[colorsNode.size()];
                    for (int i = 0; i < colors.length; i++) {
                        JsonNode c = colorsNode.get(i);
                        if (c == null || c.isNull()) {
                            colors[i] = null;
                        } else {
                            Color color = NAMED_COLORS.get(c.asText());
                            assertNotNull(color, "unknown color in cases.json: " + c.asText());
                            colors[i] = color.escape();
                        }
                    }
                }

                PieOptions options = PieOptions.builder()
                        .size(testCase.get("width").asInt(), testCase.get("height").asInt())
                        .donut(settings.path("donut").asBoolean(false))
                        .colorsAnsi(colors)
                        .showLegend(settings.path("show_legend").asBoolean(true))
                        .showPct(settings.path("show_pct").asBoolean(false))
                        .build();
                rendered = Chart.pie(slices, options);
            } else {
                JsonNode dataset = datasets.get(testCase.get("dataset").asText());

                Chart chart = testCase.get("source").asText().equals("json")
                        ? Chart.fromJson(dataset.get("json").asText())
                        : Chart.fromArrays(column(dataset, "open"), column(dataset, "high"),
                                column(dataset, "low"), column(dataset, "close"),
                                timestamps(dataset));
                try (chart) {
                    ChartOptions options = ChartOptions.builder()
                            .size(testCase.get("width").asInt(), testCase.get("height").asInt())
                            .rise(color(settings, "rise_color"))
                            .fall(color(settings, "fall_color"))
                            .background(color(settings, "bg_color"))
                            .area(color(settings, "area_color"))
                            .singleColor(settings.get("single_color").asBoolean())
                            .showPrices(settings.get("show_prices").asBoolean())
                            .showTimes(settings.get("show_times").asBoolean())
                            .plain(settings.path("plain").asBoolean(false))
                            .build();

                    rendered = testCase.get("chart").asText().equals("line")
                            ? chart.line(options)
                            : chart.candle(options);
                }
            }

            String expected = new String(
                    Files.readAllBytes(suite.resolve("golden").resolve(name + ".txt")),
                    StandardCharsets.UTF_8);
            assertEquals(expected, rendered, name + " differs from its golden file");
        }
    }
}
