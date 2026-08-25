package ccharts_test

import (
	"bytes"
	"encoding/json"
	"os"
	"path/filepath"
	"testing"

	"github.com/dethrandir/ccharts/bindings/go/ccharts"
)

// suite mirrors conformance/cases.json at the repository root: the shared
// contract every ccharts binding is held to. Only the fields used here are
// declared; the file also carries a human-readable comment.
type suite struct {
	Datasets map[string]struct {
		Open  []float64 `json:"open"`
		High  []float64 `json:"high"`
		Low   []float64 `json:"low"`
		Close []float64 `json:"close"`
		TS    []int64   `json:"ts"`
		JSON  string    `json:"json"`
	} `json:"datasets"`
	Cases []struct {
		Name    string `json:"name"`
		Dataset string `json:"dataset"`
		Source  string `json:"source"`
		Chart   string `json:"chart"`
		Width   int    `json:"width"`
		Height  int    `json:"height"`
		Slices  []struct {
			Label string  `json:"label"`
			Value float64 `json:"value"`
		} `json:"slices"`
		// Items carries the categorical (label, value) pairs for a bar chart.
		Items []struct {
			Label string  `json:"label"`
			Value float64 `json:"value"`
		} `json:"items"`
		// Series carries the 2-D stacked-bar matrix: each series has a name
		// and a values array (one entry per category).
		Series []struct {
			Name   string    `json:"name"`
			Values []float64 `json:"values"`
		} `json:"series"`
		// Samples carries the scalar histogram input (no OHLC dataset).
		Samples  []float64 `json:"samples"`
		Settings struct {
			RiseColor        *string  `json:"rise_color"`
			FallColor        *string  `json:"fall_color"`
			BgColor          *string  `json:"bg_color"`
			AreaColor        *string  `json:"area_color"`
			SingleColor      bool     `json:"single_color"`
			ShowPrices       bool     `json:"show_prices"`
			ShowTimes        bool     `json:"show_times"`
			Plain            bool     `json:"plain"`
			Donut            bool     `json:"donut"`
			Colors           []string `json:"colors"`
			ShowLegend       bool     `json:"show_legend"`
			ShowPct          bool     `json:"show_pct"`
			SliceGap         *float64 `json:"slice_gap"`
			InnerRadiusRatio *float64 `json:"inner_radius_ratio"`
			LegendFormat     *int     `json:"legend_format"`
			StartAngle       *float64 `json:"start_angle"`
			CounterClockwise *bool    `json:"counter_clockwise"`
			CenterText       *string  `json:"center_text"`
			BinCount         int      `json:"bin_count"`
			MinValue         *float64 `json:"min_value"`
			MaxValue         *float64 `json:"max_value"`
			ShowBins         bool     `json:"show_bins"`
			MinAbove         int      `json:"min_above"`
			MinBelow         int      `json:"min_below"`
			ShowLabels       bool     `json:"show_labels"`
			CatLabels        []string `json:"cat_labels"`
		} `json:"settings"`
	} `json:"cases"`
}

var namedColors = map[string]ccharts.Color{
	"black": ccharts.ColorBlack, "red": ccharts.ColorRed,
	"green": ccharts.ColorGreen, "yellow": ccharts.ColorYellow,
	"blue": ccharts.ColorBlue, "magenta": ccharts.ColorMagenta,
	"cyan": ccharts.ColorCyan, "white": ccharts.ColorWhite,
	"bright_black": ccharts.ColorBrightBlack, "bright_red": ccharts.ColorBrightRed,
	"bright_green": ccharts.ColorBrightGreen, "bright_yellow": ccharts.ColorBrightYellow,
	"bright_blue": ccharts.ColorBrightBlue, "bright_magenta": ccharts.ColorBrightMagenta,
	"bright_cyan": ccharts.ColorBrightCyan, "bright_white": ccharts.ColorBrightWhite,
	"reset": ccharts.ColorReset,
}

func color(t *testing.T, name *string) ccharts.Color {
	t.Helper()
	if name == nil {
		return ""
	}
	c, ok := namedColors[*name]
	if !ok {
		t.Fatalf("unknown color in cases.json: %q", *name)
	}
	return c
}

// deref helpers unwrap the pointer-based suite fields used to distinguish an
// absent setting from an explicit zero.
func derefF(v *float64) float64 {
	if v == nil {
		return 0
	}
	return *v
}

func derefB(v *bool) bool {
	if v == nil {
		return false
	}
	return *v
}

func derefI(v *int) int {
	if v == nil {
		return 0
	}
	return *v
}

func derefS(v *string) string {
	if v == nil {
		return ""
	}
	return *v
}

func TestConformance(t *testing.T) {
	dir := filepath.Join("..", "..", "..", "conformance")
	raw, err := os.ReadFile(filepath.Join(dir, "cases.json"))
	if err != nil {
		t.Skip("conformance suite not present; skipping")
	}

	var s suite
	if err := json.Unmarshal(raw, &s); err != nil {
		t.Fatalf("cases.json: %v", err)
	}
	if len(s.Cases) < 58 {
		t.Fatalf("conformance suite looks truncated: %d cases", len(s.Cases))
	}

	for _, tc := range s.Cases {
		t.Run(tc.Name, func(t *testing.T) {
			var got string
			var err error

			if tc.Chart == "hist" {
				got, err = ccharts.Histogram(tc.Samples, tc.Width, tc.Height,
					&ccharts.HistogramOptions{
						RiseColor:       color(t, tc.Settings.RiseColor),
						BackgroundColor: color(t, tc.Settings.BgColor),
						BinCount:        tc.Settings.BinCount,
						MinValue:        tc.Settings.MinValue,
						MaxValue:        tc.Settings.MaxValue,
						ShowBins:        tc.Settings.ShowBins,
						ShowPrices:      tc.Settings.ShowPrices,
						Plain:           tc.Settings.Plain,
					})
			} else if tc.Chart == "spark" {
				got, err = ccharts.Sparkline(tc.Samples, tc.Width, tc.Height,
					&ccharts.SparklineOptions{
						RiseColor: color(t, tc.Settings.RiseColor),
						AreaColor: color(t, tc.Settings.AreaColor),
						MinAbove:  tc.Settings.MinAbove,
						MinBelow:  tc.Settings.MinBelow,
						Plain:     tc.Settings.Plain,
					})
			} else if tc.Chart == "bar" {
				labels := make([]string, len(tc.Items))
				values := make([]float64, len(tc.Items))
				for i, item := range tc.Items {
					labels[i] = item.Label
					values[i] = item.Value
				}
				got, err = ccharts.Bar(labels, values, tc.Width, tc.Height,
					&ccharts.BarOptions{
						RiseColor:       color(t, tc.Settings.RiseColor),
						BackgroundColor: color(t, tc.Settings.BgColor),
						ShowLabels:      tc.Settings.ShowLabels,
						ShowPrices:      tc.Settings.ShowPrices,
						Plain:           tc.Settings.Plain,
					})
			} else if tc.Chart == "pie" {
				slices := make([]ccharts.Slice, len(tc.Slices))
				for i, slice := range tc.Slices {
					slices[i] = ccharts.Slice{Label: slice.Label, Value: slice.Value}
				}
				colors := make([]ccharts.Color, len(tc.Settings.Colors))
				for i, name := range tc.Settings.Colors {
					c, ok := namedColors[name]
					if !ok {
						t.Fatalf("unknown color in cases.json: %q", name)
					}
					colors[i] = c
				}
				got, err = ccharts.Pie(slices, tc.Width, tc.Height,
					&ccharts.PieOptions{
						Donut:            tc.Settings.Donut,
						Colors:           colors,
						ShowLegend:       tc.Settings.ShowLegend,
						ShowPct:          tc.Settings.ShowPct,
						SliceGap:         derefF(tc.Settings.SliceGap),
						InnerRadiusRatio: tc.Settings.InnerRadiusRatio,
						LegendFormat:     derefI(tc.Settings.LegendFormat),
						StartAngle:       tc.Settings.StartAngle,
						CounterClockwise: derefB(tc.Settings.CounterClockwise),
						CenterText:       derefS(tc.Settings.CenterText),
					})
			} else if tc.Chart == "stack" {
				series := make([]ccharts.StackSeries, len(tc.Series))
				for i, s := range tc.Series {
					series[i] = ccharts.StackSeries{Name: s.Name, Values: s.Values}
				}
				colors := make([]ccharts.Color, len(tc.Settings.Colors))
				for i, name := range tc.Settings.Colors {
					c, ok := namedColors[name]
					if !ok {
						t.Fatalf("unknown color in cases.json: %q", name)
					}
					colors[i] = c
				}
				got, err = ccharts.StackedBar(series, tc.Width, tc.Height,
					&ccharts.StackOptions{
						Colors:          colors,
						BackgroundColor: color(t, tc.Settings.BgColor),
						CategoryLabels:  tc.Settings.CatLabels,
						ShowLabels:      tc.Settings.ShowLabels,
						ShowPrices:      tc.Settings.ShowPrices,
						Plain:           tc.Settings.Plain,
					})
			} else {
				data, ok := s.Datasets[tc.Dataset]
				if !ok {
					t.Fatalf("unknown dataset %q", tc.Dataset)
				}

				var chart *ccharts.Chart
				if tc.Source == "json" {
					chart, err = ccharts.FromJSON(data.JSON)
				} else {
					chart, err = ccharts.FromArrays(data.Open, data.High, data.Low,
						data.Close, data.TS)
				}
				if err != nil {
					t.Fatalf("building the dataset: %v", err)
				}
				defer chart.Close()

				opts := &ccharts.Options{
					RiseColor:       color(t, tc.Settings.RiseColor),
					FallColor:       color(t, tc.Settings.FallColor),
					BackgroundColor: color(t, tc.Settings.BgColor),
					AreaColor:       color(t, tc.Settings.AreaColor),
					SingleColor:     tc.Settings.SingleColor,
					ShowPrices:      tc.Settings.ShowPrices,
					ShowTimes:       tc.Settings.ShowTimes,
					Plain:           tc.Settings.Plain,
				}

				if tc.Chart == "line" {
					got, err = chart.Line(tc.Width, tc.Height, opts)
				} else {
					got, err = chart.Candle(tc.Width, tc.Height, opts)
				}
			}
			if err != nil {
				t.Fatalf("rendering: %v", err)
			}

			want, err := os.ReadFile(filepath.Join(dir, "golden", tc.Name+".txt"))
			if err != nil {
				t.Fatalf("golden file: %v", err)
			}
			if !bytes.Equal([]byte(got), want) {
				t.Errorf("output differs from the golden file\n got %q\nwant %q",
					got, want)
			}
		})
	}
}
