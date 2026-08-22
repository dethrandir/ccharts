using System.Text.Json;
using Xunit;

namespace Ccharts.Tests;

/// <summary>
/// Renders the shared conformance cases and compares them byte for byte with
/// conformance/golden/*.txt — the contract every ccharts binding is held to.
/// </summary>
public class ConformanceTests
{
    private static readonly IReadOnlyDictionary<string, string> NamedColors =
        new Dictionary<string, string>
        {
            ["black"] = Color.Black, ["red"] = Color.Red, ["green"] = Color.Green,
            ["yellow"] = Color.Yellow, ["blue"] = Color.Blue,
            ["magenta"] = Color.Magenta, ["cyan"] = Color.Cyan,
            ["white"] = Color.White, ["bright_black"] = Color.BrightBlack,
            ["bright_red"] = Color.BrightRed, ["bright_green"] = Color.BrightGreen,
            ["bright_yellow"] = Color.BrightYellow, ["bright_blue"] = Color.BrightBlue,
            ["bright_magenta"] = Color.BrightMagenta,
            ["bright_cyan"] = Color.BrightCyan, ["bright_white"] = Color.BrightWhite,
            ["reset"] = Color.Reset,
        };

    /// <summary>Walks up from the test binary to the repository's suite.</summary>
    private static string? FindSuite()
    {
        var dir = AppContext.BaseDirectory;
        for (var i = 0; i < 10 && dir is not null; i++)
        {
            var candidate = Path.Combine(dir, "conformance");
            if (File.Exists(Path.Combine(candidate, "cases.json")))
            {
                return candidate;
            }
            dir = Path.GetDirectoryName(dir.TrimEnd(Path.DirectorySeparatorChar));
        }
        return null;
    }

    private static string? ColorFor(JsonElement settings, string key)
    {
        var value = settings.GetProperty(key);
        if (value.ValueKind == JsonValueKind.Null)
        {
            return null;
        }
        var name = value.GetString()!;
        Assert.True(NamedColors.ContainsKey(name), $"unknown color in cases.json: {name}");
        return NamedColors[name];
    }

    private static double[] Column(JsonElement dataset, string key) =>
        dataset.GetProperty(key).EnumerateArray().Select(v => v.GetDouble()).ToArray();

    [Fact]
    public void MatchesTheSharedGoldens()
    {
        var suiteDir = FindSuite();
        Assert.True(suiteDir is not null,
            "conformance/cases.json not found above the test binary");

        using var document = JsonDocument.Parse(
            File.ReadAllBytes(Path.Combine(suiteDir!, "cases.json")));
        var datasets = document.RootElement.GetProperty("datasets");
        var cases = document.RootElement.GetProperty("cases").EnumerateArray().ToList();
        Assert.True(cases.Count >= 10, "conformance suite looks truncated");

        foreach (var testCase in cases)
        {
            var name = testCase.GetProperty("name").GetString()!;
            var dataset = datasets.GetProperty(testCase.GetProperty("dataset").GetString()!);

            using var chart = testCase.GetProperty("source").GetString() == "json"
                ? Chart.FromJson(dataset.GetProperty("json").GetString()!)
                : Chart.FromArrays(
                    Column(dataset, "open"), Column(dataset, "high"),
                    Column(dataset, "low"), Column(dataset, "close"),
                    dataset.TryGetProperty("ts", out var ts)
                        ? ts.EnumerateArray().Select(v => v.GetInt64()).ToArray()
                        : default);

            var settings = testCase.GetProperty("settings");
            var options = new ChartOptions
            {
                Width = testCase.GetProperty("width").GetInt32(),
                Height = testCase.GetProperty("height").GetInt32(),
                RiseColor = ColorFor(settings, "rise_color"),
                FallColor = ColorFor(settings, "fall_color"),
                BackgroundColor = ColorFor(settings, "bg_color"),
                AreaColor = ColorFor(settings, "area_color"),
                SingleColor = settings.GetProperty("single_color").GetBoolean(),
                ShowPrices = settings.GetProperty("show_prices").GetBoolean(),
                ShowTimes = settings.GetProperty("show_times").GetBoolean(),
                Plain = settings.TryGetProperty("plain", out var plain)
                    && plain.GetBoolean(),
            };

            var rendered = testCase.GetProperty("chart").GetString() == "line"
                ? chart.Line(options)
                : chart.Candle(options);
            var expected = File.ReadAllText(Path.Combine(suiteDir!, "golden", name + ".txt"));

            Assert.True(rendered == expected, $"{name} differs from its golden file");
        }
    }
}
