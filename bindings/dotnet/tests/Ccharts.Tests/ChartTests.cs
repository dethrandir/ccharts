using Xunit;

namespace Ccharts.Tests;

public class ChartTests
{
    private static readonly double[] Open = [328.75, 330.0, 317.25, 320.0, 306.0];
    private static readonly double[] High = [330.0, 330.25, 321.0, 328.75, 307.25];
    private static readonly double[] Low = [323.75, 317.5, 314.5, 317.75, 300.75];
    private static readonly double[] Close = [328.0, 317.5, 321.0, 318.0, 301.0];
    private static readonly long[] Ts =
        [1784505600, 1784592000, 1784678400, 1784764800, 1784851200];

    private static Chart Sample() => Chart.FromArrays(Open, High, Low, Close, Ts);

    [Fact]
    public void RendersBothChartTypes()
    {
        using var chart = Sample();
        Assert.Equal(5, chart.Length);

        var line = chart.Line(new ChartOptions { Width = 40, Height = 4 });
        Assert.Equal(4, line.TrimEnd('\n').Split('\n').Length);

        var candle = chart.Candle(new ChartOptions { Width = 40, Height = 4 });
        Assert.Contains('│', candle);
    }

    [Fact]
    public void JsonAndArraysAgree()
    {
        const string document = """
            [{"ts":"2026-07-20T00:00:00+00:00","open":328.75,"high":330.0,"low":323.75,"close":328.0},
             {"ts":"2026-07-21T00:00:00+00:00","open":330.0,"high":330.25,"low":317.5,"close":317.5},
             {"ts":"2026-07-22T00:00:00+00:00","open":317.25,"high":321.0,"low":314.5,"close":321.0},
             {"ts":"2026-07-23T00:00:00+00:00","open":320.0,"high":328.75,"low":317.75,"close":318.0},
             {"ts":"2026-07-24T00:00:00+00:00","open":306.0,"high":307.25,"low":300.75,"close":301.0}]
            """;

        var options = new ChartOptions { ShowPrices = true, ShowTimes = true };
        using var fromJson = Chart.FromJson(document);
        using var fromArrays = Sample();
        Assert.Equal(fromArrays.Line(options), fromJson.Line(options));
        Assert.Equal(fromArrays.Candle(options), fromJson.Candle(options));
    }

    [Fact]
    public void CsvSkipsBlankLines()
    {
        using var chart = Chart.FromCsv("1,2,0.5,1.5\n\n   \n2,3,1,2.5\n");
        Assert.Equal(2, chart.Length);
    }

    [Fact]
    public void TimestampsAreOptional()
    {
        using var chart = Chart.FromArrays(Open, High, Low, Close);
        var rendered = chart.Line(new ChartOptions { Width = 20, Height = 3, ShowTimes = true });
        Assert.Equal(3, rendered.TrimEnd('\n').Split('\n').Length);
        Assert.DoesNotContain("2026", rendered);
    }

    [Fact]
    public void ColorsComeFromTheNativeLibrary()
    {
        Assert.Equal("\u001b[34m", Color.Blue);
        Assert.Equal("\u001b[97m", Color.BrightWhite);

        using var chart = Sample();
        var rendered = chart.Line(new ChartOptions
        {
            Width = 40, Height = 4, RiseColor = Color.Blue, FallColor = Color.Red,
        });
        Assert.Contains("\u001b[34m", rendered);
        Assert.Contains("\u001b[31m", rendered);
    }

    [Fact]
    public void CustomEscapesPassThrough()
    {
        using var chart = Sample();
        var rendered = chart.Line(new ChartOptions { RiseColor = "\u001b[38;5;208m" });
        Assert.Contains("\u001b[38;5;208m", rendered);
    }

    [Theory]
    [InlineData(0, 5)]
    [InlineData(5, 0)]
    [InlineData(-1, 5)]
    [InlineData(1000, 2000)]
    public void BadDimensionsAreAnErrorNotAnEmptyString(int width, int height)
    {
        using var chart = Sample();
        var error = Assert.Throws<CchartsException>(
            () => chart.Line(new ChartOptions { Width = width, Height = height }));
        Assert.Equal(CchartsStatus.Dimensions, error.Status);
    }

    [Fact]
    public void EmptyInputIsRejected()
    {
        var error = Assert.Throws<CchartsException>(
            () => Chart.FromArrays([], [], [], []));
        Assert.Equal(CchartsStatus.InvalidArgument, error.Status);
    }

    [Fact]
    public void MismatchedColumnsAreRejected()
    {
        var error = Assert.Throws<CchartsException>(
            () => Chart.FromArrays([1.0, 2.0], [2.0], [0.5, 1.0], [1.5, 2.0]));
        Assert.Equal(CchartsStatus.InvalidArgument, error.Status);
    }

    [Theory]
    [InlineData(double.NaN)]
    [InlineData(double.PositiveInfinity)]
    [InlineData(double.NegativeInfinity)]
    public void NonFinitePricesAreRejected(double bad)
    {
        var error = Assert.Throws<CchartsException>(
            () => Chart.FromArrays([bad], [2.0], [0.5], [1.5]));
        Assert.Equal(CchartsStatus.NonFinite, error.Status);
    }

    [Theory]
    [InlineData("not json")]
    [InlineData("[]")]
    [InlineData("{\"open\": 1}")]
    public void MalformedJsonIsRejected(string document)
    {
        var error = Assert.Throws<CchartsException>(() => Chart.FromJson(document));
        Assert.Equal(CchartsStatus.Parse, error.Status);
    }

    [Fact]
    public void DisposeIsIdempotentAndGuardsLaterRenders()
    {
        var chart = Sample();
        chart.Dispose();
        chart.Dispose();
        Assert.Throws<ObjectDisposedException>(() => chart.Line());
    }

    [Fact]
    public void ChartsRenderIdenticallyFromSeveralThreads()
    {
        using var chart = Sample();
        var expected = chart.Line(new ChartOptions { Width = 30, Height = 4 });
        Parallel.For(0, 16, _ =>
        {
            Assert.Equal(expected, chart.Line(new ChartOptions { Width = 30, Height = 4 }));
        });
    }

    [Fact]
    public void ExposesLibraryMetadata()
    {
        Assert.Equal("0.2.1", Chart.Version);
        Assert.Equal(100000, Chart.MaxDim);
        Assert.Equal(1000000, Chart.MaxCells);
    }

    [Fact]
    public void PieRendersDiskDonutAndEmptyForZeroValues()
    {
        PieSlice[] slices =
        [
            new("Alpha", 40),
            new("Beta", 30),
            new("Gamma", 30),
        ];

        var disk = Chart.Pie(slices, new PieOptions { ShowLegend = true, ShowPct = true });
        Assert.Contains("Alpha  40 (40%)", disk);

        var donut = Chart.Pie(slices, new PieOptions { Donut = true, ShowLegend = true });
        Assert.NotEqual(disk, donut);

        Assert.Equal(string.Empty, Chart.Pie([new PieSlice("Zero", 0)],
            new PieOptions { ShowLegend = true }));

        var error = Assert.Throws<CchartsException>(
            () => Chart.Pie([new PieSlice("Bad", double.NaN)]));
        Assert.Equal(CchartsStatus.NonFinite, error.Status);

        Assert.Throws<CchartsException>(() => Chart.Pie([]));
    }
}
