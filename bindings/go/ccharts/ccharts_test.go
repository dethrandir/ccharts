package ccharts_test

import (
	"errors"
	"math"
	"strings"
	"sync"
	"testing"

	"github.com/dethrandir/ccharts/bindings/go/ccharts"
)

var (
	sampleOpen  = []float64{328.75, 330.0, 317.25, 320.0, 306.0}
	sampleHigh  = []float64{330.0, 330.25, 321.0, 328.75, 307.25}
	sampleLow   = []float64{323.75, 317.5, 314.5, 317.75, 300.75}
	sampleClose = []float64{328.0, 317.5, 321.0, 318.0, 301.0}
	sampleTS    = []int64{1784505600, 1784592000, 1784678400, 1784764800, 1784851200}
)

func sample(t *testing.T) *ccharts.Chart {
	t.Helper()
	chart, err := ccharts.FromArrays(sampleOpen, sampleHigh, sampleLow, sampleClose, sampleTS)
	if err != nil {
		t.Fatalf("FromArrays: %v", err)
	}
	t.Cleanup(func() { chart.Close() })
	return chart
}

func TestRendersBothChartTypes(t *testing.T) {
	chart := sample(t)
	if got := chart.Len(); got != 5 {
		t.Fatalf("Len() = %d, want 5", got)
	}

	line, err := chart.Line(40, 4, nil)
	if err != nil {
		t.Fatalf("Line: %v", err)
	}
	if got := len(strings.Split(strings.TrimSuffix(line, "\n"), "\n")); got != 4 {
		t.Errorf("line has %d rows, want 4", got)
	}

	candle, err := chart.Candle(40, 4, nil)
	if err != nil {
		t.Fatalf("Candle: %v", err)
	}
	if !strings.Contains(candle, "│") {
		t.Error("candle chart should draw wicks")
	}
}

func TestJSONAndArraysAgree(t *testing.T) {
	const document = `[{"ts":"2026-07-20T00:00:00+00:00","open":328.75,"high":330.0,"low":323.75,"close":328.0},
	                   {"ts":"2026-07-21T00:00:00+00:00","open":330.0,"high":330.25,"low":317.5,"close":317.5},
	                   {"ts":"2026-07-22T00:00:00+00:00","open":317.25,"high":321.0,"low":314.5,"close":321.0},
	                   {"ts":"2026-07-23T00:00:00+00:00","open":320.0,"high":328.75,"low":317.75,"close":318.0},
	                   {"ts":"2026-07-24T00:00:00+00:00","open":306.0,"high":307.25,"low":300.75,"close":301.0}]`

	fromJSON, err := ccharts.FromJSON(document)
	if err != nil {
		t.Fatalf("FromJSON: %v", err)
	}
	defer fromJSON.Close()

	opts := &ccharts.Options{ShowPrices: true, ShowTimes: true}
	a, err := fromJSON.Line(60, 8, opts)
	if err != nil {
		t.Fatal(err)
	}
	b, err := sample(t).Line(60, 8, opts)
	if err != nil {
		t.Fatal(err)
	}
	if a != b {
		t.Error("the JSON and array entry points disagree")
	}
}

func TestCSVSkipsBlankLines(t *testing.T) {
	chart, err := ccharts.FromCSV("1,2,0.5,1.5\n\n   \n2,3,1,2.5\n", ',', '\n')
	if err != nil {
		t.Fatalf("FromCSV: %v", err)
	}
	defer chart.Close()
	if chart.Len() != 2 {
		t.Errorf("Len() = %d, want 2 (blank lines must not become zeroed candles)",
			chart.Len())
	}
}

func TestColorsComeFromTheCLibrary(t *testing.T) {
	if ccharts.ColorBlue != "\x1b[34m" {
		t.Errorf("ColorBlue = %q", ccharts.ColorBlue)
	}
	out, err := sample(t).Line(40, 4, &ccharts.Options{
		RiseColor: ccharts.ColorBlue,
		FallColor: ccharts.ColorRed,
	})
	if err != nil {
		t.Fatal(err)
	}
	if !strings.Contains(out, "\x1b[34m") || !strings.Contains(out, "\x1b[31m") {
		t.Error("both segment colors should appear")
	}
}

func TestCustomEscapeIsPassedThrough(t *testing.T) {
	out, err := sample(t).Line(40, 4, &ccharts.Options{RiseColor: "\x1b[38;5;208m"})
	if err != nil {
		t.Fatal(err)
	}
	if !strings.Contains(out, "\x1b[38;5;208m") {
		t.Error("256-color escapes should pass through unchanged")
	}
}

func TestInvalidInput(t *testing.T) {
	tests := []struct {
		name string
		fn   func() (*ccharts.Chart, error)
		want error
	}{
		{"empty", func() (*ccharts.Chart, error) {
			return ccharts.FromArrays(nil, nil, nil, nil, nil)
		}, ccharts.ErrInvalidArgument},
		{"mismatched columns", func() (*ccharts.Chart, error) {
			return ccharts.FromArrays([]float64{1, 2}, []float64{2}, []float64{0, 1},
				[]float64{1, 2}, nil)
		}, ccharts.ErrInvalidArgument},
		{"mismatched ts", func() (*ccharts.Chart, error) {
			return ccharts.FromArrays([]float64{1}, []float64{2}, []float64{0},
				[]float64{1}, []int64{1, 2})
		}, ccharts.ErrInvalidArgument},
		{"NaN", func() (*ccharts.Chart, error) {
			return ccharts.FromArrays([]float64{math.NaN()}, []float64{2},
				[]float64{0}, []float64{1}, nil)
		}, ccharts.ErrNonFinite},
		{"Inf", func() (*ccharts.Chart, error) {
			return ccharts.FromArrays([]float64{math.Inf(1)}, []float64{2},
				[]float64{0}, []float64{1}, nil)
		}, ccharts.ErrNonFinite},
		{"malformed JSON", func() (*ccharts.Chart, error) {
			return ccharts.FromJSON("not json")
		}, ccharts.ErrParse},
		{"empty JSON array", func() (*ccharts.Chart, error) {
			return ccharts.FromJSON("[]")
		}, ccharts.ErrParse},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			chart, err := tt.fn()
			if chart != nil {
				chart.Close()
				t.Fatal("expected no chart")
			}
			if !errors.Is(err, tt.want) {
				t.Errorf("err = %v, want %v", err, tt.want)
			}
		})
	}
}

func TestBadDimensionsAreAnError(t *testing.T) {
	chart := sample(t)
	for _, size := range [][2]int{{0, 5}, {5, 0}, {-1, 5}, {1000, 2000}} {
		if _, err := chart.Line(size[0], size[1], nil); !errors.Is(err, ccharts.ErrDimensions) {
			t.Errorf("Line(%d, %d) err = %v, want ErrDimensions", size[0], size[1], err)
		}
	}
}

func TestCloseIsIdempotent(t *testing.T) {
	chart, err := ccharts.FromArrays([]float64{1}, []float64{2}, []float64{0},
		[]float64{1}, nil)
	if err != nil {
		t.Fatal(err)
	}
	chart.Close()
	chart.Close()
	if _, err := chart.Line(10, 3, nil); err == nil {
		t.Error("rendering a closed chart should fail")
	}
}

func TestConcurrentRendering(t *testing.T) {
	chart := sample(t)
	want, err := chart.Line(30, 4, nil)
	if err != nil {
		t.Fatal(err)
	}

	var wg sync.WaitGroup
	for i := 0; i < 8; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			got, err := chart.Line(30, 4, nil)
			if err != nil || got != want {
				t.Errorf("concurrent render mismatch: %v", err)
			}
		}()
	}
	wg.Wait()
}

func TestMetadata(t *testing.T) {
	if ccharts.Version() != "0.2.0" {
		t.Errorf("Version() = %q", ccharts.Version())
	}
	if ccharts.MaxDim() != 100000 || ccharts.MaxCells() != 1000000 {
		t.Errorf("limits = %d, %d", ccharts.MaxDim(), ccharts.MaxCells())
	}
}
