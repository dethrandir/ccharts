// Package ccharts renders financial OHLC data to a string: line and
// candlestick charts drawn with Unicode block characters, with ANSI color
// optional.
//
// Nothing is printed for you — Line and Candle return a string, so the chart
// goes wherever text goes.
//
// It wraps the C library https://github.com/dethrandir/ccharts through its
// flat ABI. The C sources are vendored in this directory and built by cgo, so
// there is nothing to install — but cgo is required, and building with
// CGO_ENABLED=0 will not work.
//
//	chart, err := ccharts.FromArrays(opens, highs, lows, closes, epochSeconds)
//	if err != nil {
//	        return err
//	}
//	defer chart.Close()
//
//	fmt.Println(chart.Line(60, 8, &ccharts.Options{
//	        RiseColor:  ccharts.ColorBlue,
//	        ShowPrices: true,
//	        ShowTimes:  true,
//	}))
//
// A Chart is immutable once built and safe for concurrent use.
package ccharts

/*
#cgo CFLAGS: -std=c99 -O2
#cgo !windows LDFLAGS: -lm

#include <stdlib.h>
#include "ccharts_abi.h"
*/
import "C"

import (
	"errors"
	"math"
	"runtime"
	"unsafe"
)

// Error is a status code returned by the C layer. Its message comes from the
// library, so every binding reports the same wording.
type Error int

// Status codes mirroring ccharts_status in the C ABI.
const (
	errInvalidArgument Error = 1
	errParse           Error = 2
	errOutOfMemory     Error = 3
	errNonFinite       Error = 4
	errDimensions      Error = 5
)

func (e Error) Error() string {
	return C.GoString(C.ccharts_error_message(C.int32_t(e)))
}

// Sentinel errors, comparable with errors.Is.
var (
	// ErrInvalidArgument is returned for empty or mismatched input.
	ErrInvalidArgument error = errInvalidArgument
	// ErrParse is returned when JSON or CSV input cannot be read.
	ErrParse error = errParse
	// ErrOutOfMemory is returned when an allocation fails.
	ErrOutOfMemory error = errOutOfMemory
	// ErrNonFinite is returned when a price is NaN or infinite.
	ErrNonFinite error = errNonFinite
	// ErrDimensions is returned for non-positive or oversized chart sizes.
	ErrDimensions error = errDimensions
)

func statusError(status C.int32_t) error {
	if status == C.CCHARTS_OK {
		return nil
	}
	return Error(status)
}

// Color is an ANSI escape sequence. The named colors below come from the C
// library rather than being duplicated here; any other escape (256-color,
// truecolor) can be used by converting a string, e.g.
// Color("\x1b[38;5;208m"). The zero value means "library default".
type Color string

func colorAt(index int) Color {
	return Color(C.GoString(C.ccharts_color(C.int32_t(index))))
}

// The sixteen ANSI colors and the reset sequence.
var (
	ColorBlack         = colorAt(0)
	ColorRed           = colorAt(1)
	ColorGreen         = colorAt(2)
	ColorYellow        = colorAt(3)
	ColorBlue          = colorAt(4)
	ColorMagenta       = colorAt(5)
	ColorCyan          = colorAt(6)
	ColorWhite         = colorAt(7)
	ColorBrightBlack   = colorAt(8)
	ColorBrightRed     = colorAt(9)
	ColorBrightGreen   = colorAt(10)
	ColorBrightYellow  = colorAt(11)
	ColorBrightBlue    = colorAt(12)
	ColorBrightMagenta = colorAt(13)
	ColorBrightCyan    = colorAt(14)
	ColorBrightWhite   = colorAt(15)
	ColorReset         = colorAt(16)
)

// Options controls how a chart is drawn. The zero value takes every default:
// green rising, red falling, no background, no area fill, no labels.
type Options struct {
	// RiseColor colors rising values and candles (default green).
	RiseColor Color
	// FallColor colors falling values and candles (default red).
	FallColor Color
	// BackgroundColor fills empty cells (default: the terminal background).
	BackgroundColor Color
	// AreaColor fills the area below a line chart (default: nothing).
	AreaColor Color
	// SingleColor draws the whole chart in one color chosen from the overall
	// change instead of coloring each segment by its own direction.
	SingleColor bool
	// ShowPrices prints max/min price labels in a left margin.
	ShowPrices bool
	// ShowTimes prints the first and last timestamp under the chart.
	ShowTimes bool
	// Plain renders with no ANSI escapes at all, overriding every color. Use
	// it when the chart is going somewhere that does not interpret escapes —
	// a log file, an HTML block, a commit message.
	Plain bool
}

// Slice is one slice of a pie chart: a label (may be empty) and a positive
// amount. The pie computes the percentage from the sum of the values.
type Slice struct {
	// Label is drawn in the legend when ShowLegend is set.
	Label string
	// Value is a positive amount; a value <= 0 makes the whole render return
	// the empty string rather than an error.
	Value float64
}

// PieOptions controls how a pie chart is drawn. A nil *PieOptions behaves
// like &PieOptions{ShowLegend: true}: a filled disk, no override colors, a
// legend without percentages.
type PieOptions struct {
	// Donut hollows out the center (a donut) instead of a filled disk.
	Donut bool
	// Colors overrides the per-slice palette, one ANSI escape per slice
	// (slice i uses Colors[i%len(Colors)]); nil selects the fixed default
	// palette.
	Colors []Color
	// ShowLegend prints one "label  value (pct%)" line per slice below the
	// disk.
	ShowLegend bool
	// ShowPct appends the "(NN%)" to each legend entry.
	ShowPct bool
	// SliceGap is the angular gap between slices, in radians. 0 keeps the
	// slices adjacent, producing thin gaps when applied to a donut.
	SliceGap float64
	// InnerRadiusRatio is the donut thickness in [0, 1]: 0 = a filled disk,
	// 1 = a hairline ring. nil (unspecified) leaves it to Donut (0.5 for a
	// donut, 0 for a disk). >1 is clamped to 1.
	InnerRadiusRatio *float64
	// LegendFormat selects the legend entry format:
	// 0 = "label  value" (+ "(NN%)" with ShowPct),
	// 1 = "label  NN%", 2 = "value  (NN%)", 3 = "label" only. Unknown values
	// fall back to 0.
	LegendFormat int
	// StartAngle is the angle in radians at which slice 0 begins. nil
	// (unspecified) uses the library default (12 o'clock).
	StartAngle *float64
	// CounterClockwise sweeps the slices clockwise instead of the default
	// counter-clockwise.
	CounterClockwise bool
	// CenterText is drawn in the hollow center of a donut (only when there is
	// a hollow); "" disables it.
	CenterText string
}

// Chart is a parsed OHLC dataset that can be rendered repeatedly.
//
// Close releases the underlying C memory. A finalizer does the same if Close
// is never called, but relying on it keeps the memory alive longer than
// necessary.
type Chart struct {
	handle *C.ccharts_data
	n      int
}

func wrap(status C.int32_t, handle *C.ccharts_data) (*Chart, error) {
	if err := statusError(status); err != nil {
		return nil, err
	}
	if handle == nil {
		return nil, ErrOutOfMemory
	}
	chart := &Chart{handle: handle, n: int(C.ccharts_data_len(handle))}
	runtime.SetFinalizer(chart, (*Chart).Close)
	return chart, nil
}

// FromArrays builds a chart from four equal-length price columns. ts holds
// epoch seconds and may be nil. The data is copied into C memory during the
// call and no Go pointer is retained afterwards.
func FromArrays(open, high, low, close []float64, ts []int64) (*Chart, error) {
	n := len(open)
	if n == 0 {
		return nil, ErrInvalidArgument
	}
	if len(high) != n || len(low) != n || len(close) != n {
		return nil, ErrInvalidArgument
	}
	if ts != nil && len(ts) != n {
		return nil, ErrInvalidArgument
	}

	var tsPtr *C.int64_t
	if ts != nil {
		tsPtr = (*C.int64_t)(unsafe.Pointer(&ts[0]))
	}

	var handle *C.ccharts_data
	status := C.ccharts_from_arrays(
		(*C.double)(unsafe.Pointer(&open[0])),
		(*C.double)(unsafe.Pointer(&high[0])),
		(*C.double)(unsafe.Pointer(&low[0])),
		(*C.double)(unsafe.Pointer(&close[0])),
		tsPtr,
		C.int32_t(n),
		&handle,
	)
	runtime.KeepAlive(open)
	runtime.KeepAlive(high)
	runtime.KeepAlive(low)
	runtime.KeepAlive(close)
	runtime.KeepAlive(ts)
	return wrap(status, handle)
}

// FromJSON builds a chart from the fixed-schema JSON document described in the
// project README: an array of objects with ts, open, high, low and close.
func FromJSON(document string) (*Chart, error) {
	cs := C.CString(document)
	defer C.free(unsafe.Pointer(cs))

	var handle *C.ccharts_data
	return wrap(C.ccharts_parse_json(cs, &handle), handle)
}

// FromCSV builds a chart from rows of open,high,low,close[,timestamp].
// Blank lines are skipped.
func FromCSV(text string, valueSeparator, lineSeparator byte) (*Chart, error) {
	if valueSeparator == 0 || lineSeparator == 0 {
		return nil, ErrInvalidArgument
	}
	cs := C.CString(text)
	defer C.free(unsafe.Pointer(cs))

	var handle *C.ccharts_data
	status := C.ccharts_parse_csv(cs, C.char(valueSeparator), C.char(lineSeparator), &handle)
	return wrap(status, handle)
}

// Len reports the number of candles in the dataset.
func (c *Chart) Len() int { return c.n }

// Close releases the dataset. It is safe to call more than once.
func (c *Chart) Close() error {
	if c.handle != nil {
		C.ccharts_data_free(c.handle)
		c.handle = nil
		runtime.SetFinalizer(c, nil)
	}
	return nil
}

// Line renders a line chart of the closing prices. opts may be nil.
func (c *Chart) Line(width, height int, opts *Options) (string, error) {
	return c.render(true, width, height, opts)
}

// Candle renders a candlestick chart. opts may be nil.
func (c *Chart) Candle(width, height int, opts *Options) (string, error) {
	return c.render(false, width, height, opts)
}

func (c *Chart) render(line bool, width, height int, opts *Options) (string, error) {
	if c.handle == nil {
		return "", errors.New("ccharts: chart is closed")
	}
	if width <= 0 || height <= 0 || width > int(C.ccharts_max_dim()) ||
		height > int(C.ccharts_max_dim()) {
		return "", ErrDimensions
	}

	var settings C.ccharts_settings
	free := setColors(&settings, opts)
	defer free()

	var out *C.char
	var length C.size_t
	var status C.int32_t
	if line {
		status = C.ccharts_line(c.handle, C.int32_t(width), C.int32_t(height),
			&settings, &out, &length)
	} else {
		status = C.ccharts_candle(c.handle, C.int32_t(width), C.int32_t(height),
			&settings, &out, &length)
	}
	if err := statusError(status); err != nil {
		return "", err
	}
	defer C.ccharts_string_free(out)
	return C.GoStringN(out, C.int(length)), nil
}

// Pie renders a pie/donut chart from the given slices. A pie has no OHLC
// dataset, so this is a package function rather than a Chart method.
//
// opts may be nil. All-zero or non-positive slice values produce the empty
// string (not an error); NaN and inf are rejected with ErrNonFinite.
func Pie(slices []Slice, width, height int, opts *PieOptions) (string, error) {
	if len(slices) == 0 {
		return "", ErrInvalidArgument
	}
	if width <= 0 || height <= 0 || width > int(C.ccharts_max_dim()) ||
		height > int(C.ccharts_max_dim()) {
		return "", ErrDimensions
	}

	n := len(slices)
	cSlices := make([]C.ccharts_pie_slice, n)
	labels := make([]*C.char, n)
	for i, s := range slices {
		labels[i] = C.CString(s.Label)
		cSlices[i].label = labels[i]
		cSlices[i].value = C.double(s.Value)
	}
	defer func() {
		for _, cs := range labels {
			C.free(unsafe.Pointer(cs))
		}
	}()

	var colors **C.char
	var colorCount C.int32_t
	var ownedColors []*C.char
	if opts != nil && len(opts.Colors) > 0 {
		ownedColors = make([]*C.char, len(opts.Colors))
		ptrs := make([]*C.char, len(opts.Colors))
		for i, c := range opts.Colors {
			ownedColors[i] = C.CString(string(c))
			ptrs[i] = ownedColors[i]
		}
		colors = &ptrs[0]
		colorCount = C.int32_t(len(ptrs))
		defer func() {
			for _, cs := range ownedColors {
				C.free(unsafe.Pointer(cs))
			}
		}()
	}

	var donut C.int32_t
	var showLegend C.int32_t
	var showPct C.int32_t
	var sliceGap C.double
	var legendFormat C.int32_t
	var counterClockwise C.int32_t
	if opts != nil {
		donut = boolToC(opts.Donut)
		showLegend = boolToC(opts.ShowLegend)
		showPct = boolToC(opts.ShowPct)
		sliceGap = C.double(opts.SliceGap)
		legendFormat = C.int32_t(opts.LegendFormat)
		counterClockwise = boolToC(opts.CounterClockwise)
	}
	// A nil pointer is the "unspecified" sentinel, which the C layer maps to
	// the default (Donut decides InnerRadiusRatio, 12 o'clock for
	// StartAngle).
	var innerRatio C.double = -1
	if opts != nil && opts.InnerRadiusRatio != nil {
		innerRatio = C.double(*opts.InnerRadiusRatio)
	}
	var startAngle C.double = -1
	if opts != nil && opts.StartAngle != nil {
		startAngle = C.double(*opts.StartAngle)
	}

	var centerTextPtr *C.char
	if opts != nil && opts.CenterText != "" {
		centerTextPtr = C.CString(opts.CenterText)
		defer C.free(unsafe.Pointer(centerTextPtr))
	}

	var out *C.char
	var length C.size_t
	status := C.ccharts_pie_from_slices(
		&cSlices[0], C.int32_t(n), C.int32_t(width), C.int32_t(height),
		donut, colors, colorCount, showLegend, showPct,
		sliceGap, innerRatio, legendFormat, startAngle, counterClockwise,
		centerTextPtr, &out, &length)
	if err := statusError(status); err != nil {
		return "", err
	}
	defer C.ccharts_string_free(out)
	return C.GoStringN(out, C.int(length)), nil
}

// HistogramOptions controls how a histogram is drawn. A nil *HistogramOptions
// behaves like &HistogramOptions{}: green bars, auto bin count, auto value
// range from the data, no footer or margin.
type HistogramOptions struct {
	// RiseColor colors the bars (default green).
	RiseColor Color
	// BackgroundColor fills empty cells (default: the terminal background).
	BackgroundColor Color
	// BinCount is the number of equal-width bins; 0 selects a value from the
	// sample count, bounded by the chart width.
	BinCount int
	// MinValue is the lower edge of the value window; nil uses the data
	// minimum. When set, must be strictly less than MaxValue.
	MinValue *float64
	// MaxValue is the upper edge of the value window; nil uses the data
	// maximum. When set, must be strictly greater than MinValue.
	MaxValue *float64
	// ShowBins appends a value-axis footer row (window min left, window max
	// right).
	ShowBins bool
	// ShowPrices prints the max-count / min-count labels in a left margin.
	ShowPrices bool
	// Plain renders with no ANSI escapes at all, overriding every color.
	Plain bool
}

// Histogram renders a histogram of the given scalar samples. A histogram has
// no OHLC dataset, so this is a package function rather than a Chart method.
//
// opts may be nil. NaN and inf samples are rejected with ErrNonFinite.
func Histogram(samples []float64, width, height int, opts *HistogramOptions) (string, error) {
	if len(samples) == 0 {
		return "", ErrInvalidArgument
	}
	if width <= 0 || height <= 0 || width > int(C.ccharts_max_dim()) ||
		height > int(C.ccharts_max_dim()) {
		return "", ErrDimensions
	}

	var settings C.ccharts_hist_settings
	settings.bin_count = C.int32_t(0)
	settings.min_value = C.double(math.NaN())
	settings.max_value = C.double(math.NaN())
	if opts != nil {
		settings.bin_count = C.int32_t(opts.BinCount)
		if opts.MinValue != nil {
			settings.min_value = C.double(*opts.MinValue)
		}
		if opts.MaxValue != nil {
			settings.max_value = C.double(*opts.MaxValue)
		}
		settings.show_bins = boolToC(opts.ShowBins)
		settings.show_prices = boolToC(opts.ShowPrices)
	}
	var allocated []*C.char
	defer func() {
		for _, cs := range allocated {
			C.free(unsafe.Pointer(cs))
		}
	}()
	set := func(dst **C.char, color Color) {
		if opts == nil {
			return
		}
		if opts.Plain {
			cs := C.CString("")
			allocated = append(allocated, cs)
			*dst = cs
			return
		}
		if color == "" {
			return
		}
		cs := C.CString(string(color))
		allocated = append(allocated, cs)
		*dst = cs
	}
	set(&settings.rise_color, opts.RiseColor)
	if opts != nil {
		set(&settings.bg_color, opts.BackgroundColor)
	} else {
		settings.bg_color = nil
	}

	var out *C.char
	var length C.size_t
	status := C.ccharts_hist(
		(*C.double)(unsafe.Pointer(&samples[0])), C.int32_t(len(samples)),
		C.int32_t(width), C.int32_t(height),
		&settings, &out, &length)
	runtime.KeepAlive(samples)
	if err := statusError(status); err != nil {
		return "", err
	}
	defer C.ccharts_string_free(out)
	return C.GoStringN(out, C.int(length)), nil
}

// SparklineOptions controls how a sparkline is drawn. A nil *SparklineOptions
// behaves like &SparklineOptions{}: a green line, no area fill, no reserved
// edge margins.
type SparklineOptions struct {
	// RiseColor colors the trend line (default green).
	RiseColor Color
	// AreaColor fills the area under the line (default: nothing).
	AreaColor Color
	// MinAbove reserves this many sub-pixels at the top edge so the line
	// does not clip at the very top of a tiny chart.
	MinAbove int
	// MinBelow reserves this many sub-pixels at the bottom edge so the line
	// does not clip at the very bottom of a tiny chart.
	MinBelow int
	// Plain renders with no ANSI escapes at all, overriding every color.
	Plain bool
}

// Sparkline renders a sparkline of the given scalar samples. A sparkline has
// no OHLC dataset, so this is a package function rather than a Chart method.
//
// opts may be nil. NaN and inf samples are rejected with ErrNonFinite.
func Sparkline(samples []float64, width, height int, opts *SparklineOptions) (string, error) {
	if len(samples) == 0 {
		return "", ErrInvalidArgument
	}
	if width <= 0 || height <= 0 || width > int(C.ccharts_max_dim()) ||
		height > int(C.ccharts_max_dim()) {
		return "", ErrDimensions
	}

	var settings C.ccharts_spark_settings
	var allocated []*C.char
	defer func() {
		for _, cs := range allocated {
			C.free(unsafe.Pointer(cs))
		}
	}()
	set := func(dst **C.char, color Color) {
		if opts == nil {
			return
		}
		if opts.Plain {
			cs := C.CString("")
			allocated = append(allocated, cs)
			*dst = cs
			return
		}
		if color == "" {
			return
		}
		cs := C.CString(string(color))
		allocated = append(allocated, cs)
		*dst = cs
	}
	set(&settings.rise_color, opts.RiseColor)
	set(&settings.area_color, opts.AreaColor)
	if opts != nil {
		settings.min_above = C.int32_t(opts.MinAbove)
		settings.min_below = C.int32_t(opts.MinBelow)
	}

	var out *C.char
	var length C.size_t
	status := C.ccharts_spark(
		(*C.double)(unsafe.Pointer(&samples[0])), C.int32_t(len(samples)),
		C.int32_t(width), C.int32_t(height),
		&settings, &out, &length)
	runtime.KeepAlive(samples)
	if err := statusError(status); err != nil {
		return "", err
	}
	defer C.ccharts_string_free(out)
	return C.GoStringN(out, C.int(length)), nil
}

// BarOptions controls how a bar chart is drawn. A nil *BarOptions behaves
// like &BarOptions{}: green bars, no background, no label or value footer.
type BarOptions struct {
	// RiseColor colors the bars (default green).
	RiseColor Color
	// BackgroundColor fills empty cells (default: the terminal background).
	BackgroundColor Color
	// ShowLabels prints each column's label in a footer row below the chart.
	ShowLabels bool
	// ShowPrices prints the max bar value and 0 (the baseline) in a left
	// value-axis margin.
	ShowPrices bool
	// Plain renders with no ANSI escapes at all, overriding every color.
	Plain bool
}

// Bar renders a categorical bar chart of the (label, value) pairs formed by
// the parallel labels and values arrays. A bar chart has no OHLC dataset, so
// this is a package function rather than a Chart method.
//
// labels and values must have the same length. opts may be nil. Negative
// values are clamped to zero by the renderer; NaN and inf are rejected with
// ErrNonFinite.
func Bar(labels []string, values []float64, width, height int, opts *BarOptions) (string, error) {
	if len(labels) == 0 {
		return "", ErrInvalidArgument
	}
	if len(labels) != len(values) {
		return "", ErrInvalidArgument
	}
	if width <= 0 || height <= 0 || width > int(C.ccharts_max_dim()) ||
		height > int(C.ccharts_max_dim()) {
		return "", ErrDimensions
	}

	n := len(labels)
	cItems := make([]C.ccharts_bar_slice, n)
	owned := make([]*C.char, n)
	for i := range labels {
		owned[i] = C.CString(labels[i])
		cItems[i].label = owned[i]
		cItems[i].value = C.double(values[i])
	}
	defer func() {
		for _, cs := range owned {
			C.free(unsafe.Pointer(cs))
		}
	}()

	var settings C.ccharts_bar_settings
	var allocated []*C.char
	defer func() {
		for _, cs := range allocated {
			C.free(unsafe.Pointer(cs))
		}
	}()
	set := func(dst **C.char, color Color) {
		if opts == nil {
			return
		}
		if opts.Plain {
			cs := C.CString("")
			allocated = append(allocated, cs)
			*dst = cs
			return
		}
		if color == "" {
			return
		}
		cs := C.CString(string(color))
		allocated = append(allocated, cs)
		*dst = cs
	}
	set(&settings.rise_color, opts.RiseColor)
	set(&settings.bg_color, opts.BackgroundColor)
	if opts != nil {
		settings.show_labels = boolToC(opts.ShowLabels)
		settings.show_prices = boolToC(opts.ShowPrices)
	}

	var out *C.char
	var length C.size_t
	status := C.ccharts_bar(
		&cItems[0], C.int32_t(n), C.int32_t(width), C.int32_t(height),
		&settings, &out, &length)
	if err := statusError(status); err != nil {
		return "", err
	}
	defer C.ccharts_string_free(out)
	return C.GoStringN(out, C.int(length)), nil
}

// StackSeries is one series of a stacked bar chart: a name (used for
// documentation; the per-series color comes from the palette) and one value
// per category.
type StackSeries struct {
	// Name identifies the series.
	Name string
	// Values holds one entry per category.
	Values []float64
}

// StackOptions controls how a stacked bar chart is drawn. A nil
// *StackOptions behaves like &StackOptions{}: the fixed deterministic
// per-series palette, no background, no category-label or value footer.
type StackOptions struct {
	// Colors overrides the per-series palette, one ANSI escape per series
	// (series i uses Colors[i%len(Colors)]); nil selects the fixed default
	// palette.
	Colors []Color
	// BackgroundColor fills empty cells above the tallest stack (default:
	// the terminal background).
	BackgroundColor Color
	// CategoryLabels names each output column in the footer when ShowLabels
	// is set (nil = no category names).
	CategoryLabels []string
	// ShowLabels appends a footer row with each column's category label.
	ShowLabels bool
	// ShowPrices prints the tallest stack total and 0 (the baseline) in a
	// left value-axis margin.
	ShowPrices bool
	// Plain renders with no ANSI escapes at all, overriding every color.
	Plain bool
}

// StackedBar renders a stacked bar chart of the given series. Every series
// must carry the same number of values (one per category); each category's
// bar is the vertical SUM of its series' values, drawn as stacked segments.
// A stacked bar has no OHLC dataset, so this is a package function rather
// than a Chart method.
//
// opts may be nil. Negative values are clamped to zero by the renderer; NaN
// and inf are rejected with ErrNonFinite.
func StackedBar(series []StackSeries, width, height int, opts *StackOptions) (string, error) {
	if len(series) == 0 {
		return "", ErrInvalidArgument
	}
	cats := len(series[0].Values)
	if cats == 0 {
		return "", ErrInvalidArgument
	}
	for _, s := range series {
		if len(s.Values) != cats {
			return "", ErrInvalidArgument
		}
	}
	if width <= 0 || height <= 0 || width > int(C.ccharts_max_dim()) ||
		height > int(C.ccharts_max_dim()) {
		return "", ErrDimensions
	}

	var allocated []*C.char
	defer func() {
		for _, cs := range allocated {
			C.free(unsafe.Pointer(cs))
		}
	}()
	strPtr := func(s string) *C.char {
		cs := C.CString(s)
		allocated = append(allocated, cs)
		return cs
	}

	n := len(series)
	cSeries := make([]C.ccharts_stack_series, n)
	// Each series' values must be in C memory: cgo rejects Go-heap pointers
	// nested inside a struct handed to a C call, and the library reads the
	// values through the per-series pointer.
	var cValues []unsafe.Pointer
	defer func() {
		for _, p := range cValues {
			C.free(p)
		}
	}()
	for i := range series {
		cSeries[i].name = strPtr(series[i].Name)
		buf := C.malloc(C.size_t(cats) * C.size_t(unsafe.Sizeof(float64(0))))
		if buf == nil {
			return "", ErrOutOfMemory
		}
		cValues = append(cValues, buf)
		dest := unsafe.Slice((*C.double)(buf), cats)
		for j := 0; j < cats; j++ {
			dest[j] = C.double(series[i].Values[j])
		}
		cSeries[i].values = &dest[0]
	}

	var settings C.ccharts_stack_settings
	set := func(dst **C.char, color Color) {
		if opts == nil {
			return
		}
		if opts.Plain {
			*dst = strPtr("")
			return
		}
		if color == "" {
			return
		}
		*dst = strPtr(string(color))
	}
	if opts != nil {
		set(&settings.bg_color, opts.BackgroundColor)
	}

	// Copy C strings into a freshly allocated, C-owned array of pointers
	// (NULL-terminated). The settings struct holds the array pointer, so it
	// must be C memory — cgo rejects Go-heap pointers nested inside a struct
	// handed to a C call.
	allocPtrs := func(ptrs []*C.char) **C.char {
		if len(ptrs) == 0 {
			return nil
		}
		buf := C.malloc(C.size_t(len(ptrs)+1) * C.size_t(unsafe.Sizeof((*C.char)(nil))))
		if buf == nil {
			return nil
		}
		cValues = append(cValues, buf)
		arr := unsafe.Slice((**C.char)(buf), len(ptrs)+1)
		for i, p := range ptrs {
			arr[i] = p
		}
		arr[len(ptrs)] = nil
		return &arr[0]
	}

	// A NULL-terminated per-series palette; `plain` injects one empty escape
	// per series so the render emits no ANSI codes at all.
	var palette []*C.char
	if opts != nil && opts.Plain {
		for i := 0; i < n; i++ {
			palette = append(palette, strPtr(""))
		}
	} else if opts != nil {
		for _, c := range opts.Colors {
			palette = append(palette, strPtr(string(c)))
		}
	}
	if len(palette) > 0 {
		settings.colors = allocPtrs(palette)
	}

	if opts != nil && len(opts.CategoryLabels) > 0 {
		labels := make([]*C.char, len(opts.CategoryLabels))
		for i, l := range opts.CategoryLabels {
			labels[i] = strPtr(l)
		}
		settings.cat_labels = allocPtrs(labels)
	}
	settings.series = C.int32_t(n)
	settings.cats = C.int32_t(cats)
	if opts != nil {
		settings.show_labels = boolToC(opts.ShowLabels)
		settings.show_prices = boolToC(opts.ShowPrices)
	}

	var out *C.char
	var length C.size_t
	status := C.ccharts_stack(
		&cSeries[0], C.int32_t(n), C.int32_t(width), C.int32_t(height),
		&settings, &out, &length)
	if err := statusError(status); err != nil {
		return "", err
	}
	defer C.ccharts_string_free(out)
	return C.GoStringN(out, C.int(length)), nil
}

// HeatOptions controls how a heatmap is drawn. A nil *HeatOptions behaves
// like &HeatOptions{}: the fixed deterministic colormap ladder, no
// background, no row/column labels.
type HeatOptions struct {
	// LowColor is the ANSI color for the matrix minimum (default the ladder's
	// low end).
	LowColor Color
	// HighColor is the ANSI color for the matrix maximum (default the
	// ladder's high end).
	HighColor Color
	// MidColor optionally replaces the ladder's middle entry with a 3-stop
	// ramp (zero value = 2-stop).
	MidColor Color
	// BackgroundColor colors the grid cells the matrix does not cover (a
	// matrix smaller than the grid); default: the terminal background.
	BackgroundColor Color
	// RowLabels labels each matrix row around the grid when ShowLabels is
	// set (nil = no labels).
	RowLabels []string
	// ColLabels labels each matrix column around the grid when ShowLabels is
	// set (nil = no labels).
	ColLabels []string
	// ShowLabels prints the row/column labels around the grid.
	ShowLabels bool
	// Plain renders with no ANSI escapes at all, overriding every color.
	Plain bool
}

// Heatmap renders a heatmap of a rows x cols row-major values matrix into a
// width x height grid. Every row of the matrix must share the same length.
// A heatmap has no OHLC dataset, so this is a package function rather than a
// Chart method.
//
// opts may be nil. The colormap ladder is deterministic in the C core; the
// matrix elements just map to it by their position between the matrix min and
// max. NaN and inf values are rejected with ErrNonFinite.
func Heatmap(values [][]float64, width, height int, opts *HeatOptions) (string, error) {
	if len(values) == 0 {
		return "", ErrInvalidArgument
	}
	cols := len(values[0])
	if cols == 0 {
		return "", ErrInvalidArgument
	}
	for _, row := range values {
		if len(row) != cols {
			return "", ErrInvalidArgument
		}
	}
	if width <= 0 || height <= 0 || width > int(C.ccharts_max_dim()) ||
		height > int(C.ccharts_max_dim()) {
		return "", ErrDimensions
	}

	rows := len(values)
	var cAlloc []unsafe.Pointer
	defer func() {
		for _, p := range cAlloc {
			C.free(p)
		}
	}()

	// Flatten the matrix row-major into a contiguous C buffer. cgo rejects
	// Go-heap pointers nested inside a struct handed to a C call, and the
	// values come as a single double* the library reads directly, so the
	// whole matrix lives in C memory for the call.
	buf := C.malloc(C.size_t(rows*cols) * C.size_t(unsafe.Sizeof(float64(0))))
	if buf == nil {
		return "", ErrOutOfMemory
	}
	cAlloc = append(cAlloc, buf)
	flat := unsafe.Slice((*C.double)(buf), rows*cols)
	idx := 0
	for _, row := range values {
		for _, v := range row {
			flat[idx] = C.double(v)
			idx++
		}
	}

	var allocated []*C.char
	defer func() {
		for _, cs := range allocated {
			C.free(unsafe.Pointer(cs))
		}
	}()
	strPtr := func(s string) *C.char {
		cs := C.CString(s)
		allocated = append(allocated, cs)
		return cs
	}
	set := func(dst **C.char, color Color) {
		if opts == nil {
			return
		}
		if opts.Plain {
			*dst = strPtr("")
			return
		}
		if color == "" {
			return
		}
		*dst = strPtr(string(color))
	}

	var settings C.ccharts_heat_settings
	if opts != nil {
		set(&settings.low_color, opts.LowColor)
		set(&settings.high_color, opts.HighColor)
		set(&settings.mid_color, opts.MidColor)
		set(&settings.bg_color, opts.BackgroundColor)
		settings.show_labels = boolToC(opts.ShowLabels)
	}

	// Copy the label strings into freshly allocated, C-owned arrays of
	// pointers. The settings struct holds the array pointers, so they must be
	// C memory — cgo rejects Go-heap pointers nested inside a struct handed
	// to a C call.
	allocPtrs := func(ptrs []*C.char) **C.char {
		if len(ptrs) == 0 {
			return nil
		}
		arrBuf := C.malloc(C.size_t(len(ptrs)+1) * C.size_t(unsafe.Sizeof((*C.char)(nil))))
		if arrBuf == nil {
			return nil
		}
		cAlloc = append(cAlloc, arrBuf)
		arr := unsafe.Slice((**C.char)(arrBuf), len(ptrs)+1)
		for i, p := range ptrs {
			arr[i] = p
		}
		arr[len(ptrs)] = nil
		return &arr[0]
	}

	if opts != nil && len(opts.RowLabels) > 0 {
		ptrs := make([]*C.char, len(opts.RowLabels))
		for i, l := range opts.RowLabels {
			ptrs[i] = strPtr(l)
		}
		settings.row_labels = allocPtrs(ptrs)
	}
	if opts != nil && len(opts.ColLabels) > 0 {
		ptrs := make([]*C.char, len(opts.ColLabels))
		for i, l := range opts.ColLabels {
			ptrs[i] = strPtr(l)
		}
		settings.col_labels = allocPtrs(ptrs)
	}

	var out *C.char
	var length C.size_t
	status := C.ccharts_heat(
		(*C.double)(buf), C.int32_t(rows), C.int32_t(cols),
		C.int32_t(width), C.int32_t(height),
		&settings, &out, &length)
	if err := statusError(status); err != nil {
		return "", err
	}
	defer C.ccharts_string_free(out)
	return C.GoStringN(out, C.int(length)), nil
}

// BoxCategory is one category of a box plot: a name (the core does not print
// it) and that category's samples.
type BoxCategory struct {
	// Name identifies the category.
	Name string
	// Samples holds this category's sample values.
	Samples []float64
}

// BoxOptions controls how a box plot is drawn. A nil *BoxOptions behaves
// like &BoxOptions{}: green box/median, whiskers sharing the box color, no
// background, no value axis.
type BoxOptions struct {
	// RiseColor colors the box and median line (default green).
	RiseColor Color
	// AreaColor colors the whiskers (zero value = share RiseColor).
	AreaColor Color
	// BackgroundColor fills the empty cells above/below a box (default: the
	// terminal background).
	BackgroundColor Color
	// ShowPrices prints the global max/min value labels in a left margin.
	ShowPrices bool
	// Plain renders with no ANSI escapes at all, overriding every color.
	Plain bool
}

// Boxplot renders a box plot of the given categories. Each category carries
// its own (possibly ragged) samples array; the C core computes a nearest-rank
// five-number summary per category and draws each box and its whiskers over
// the global min/max span, so the binding passes raw samples and settings
// only. A box plot has no OHLC dataset, so this is a package function rather
// than a Chart method.
//
// opts may be nil. NaN and inf samples are rejected with ErrNonFinite.
func Boxplot(series []BoxCategory, width, height int, opts *BoxOptions) (string, error) {
	if len(series) == 0 {
		return "", ErrInvalidArgument
	}
	for _, c := range series {
		if len(c.Samples) == 0 {
			return "", ErrInvalidArgument
		}
	}
	if width <= 0 || height <= 0 || width > int(C.ccharts_max_dim()) ||
		height > int(C.ccharts_max_dim()) {
		return "", ErrDimensions
	}

	var allocated []*C.char
	defer func() {
		for _, cs := range allocated {
			C.free(unsafe.Pointer(cs))
		}
	}()
	strPtr := func(s string) *C.char {
		cs := C.CString(s)
		allocated = append(allocated, cs)
		return cs
	}

	// Each category's samples must be in C memory: cgo rejects Go-heap
	// pointers nested inside a struct handed to a C call, and the library
	// reads the samples through the per-category pointer.
	var cSamples []unsafe.Pointer
	defer func() {
		for _, p := range cSamples {
			C.free(p)
		}
	}()
	cCats := make([]C.ccharts_box_category, len(series))
	for i := range series {
		cCats[i].name = strPtr(series[i].Name)
		n := len(series[i].Samples)
		buf := C.malloc(C.size_t(n) * C.size_t(unsafe.Sizeof(float64(0))))
		if buf == nil {
			return "", ErrOutOfMemory
		}
		cSamples = append(cSamples, buf)
		dest := unsafe.Slice((*C.double)(buf), n)
		for j := 0; j < n; j++ {
			dest[j] = C.double(series[i].Samples[j])
		}
		cCats[i].samples = &dest[0]
		cCats[i].n = C.int32_t(n)
	}

	var settings C.ccharts_box_settings
	set := func(dst **C.char, color Color) {
		if opts == nil {
			return
		}
		if opts.Plain {
			*dst = strPtr("")
			return
		}
		if color == "" {
			return
		}
		*dst = strPtr(string(color))
	}
	if opts != nil {
		set(&settings.rise_color, opts.RiseColor)
		set(&settings.area_color, opts.AreaColor)
		set(&settings.bg_color, opts.BackgroundColor)
		settings.show_prices = boolToC(opts.ShowPrices)
	}

	var out *C.char
	var length C.size_t
	status := C.ccharts_box(
		&cCats[0], C.int32_t(len(series)), C.int32_t(width), C.int32_t(height),
		&settings, &out, &length)
	if err := statusError(status); err != nil {
		return "", err
	}
	defer C.ccharts_string_free(out)
	return C.GoStringN(out, C.int(length)), nil
}

func setColors(settings *C.ccharts_settings, opts *Options) func() {
	if opts == nil {
		return func() {}
	}

	var allocated []*C.char
	set := func(dst **C.char, color Color) {
		// An empty C string tells the library to emit no escape at all, which
		// is different from a NULL pointer (use the default color).
		if opts.Plain {
			cs := C.CString("")
			allocated = append(allocated, cs)
			*dst = cs
			return
		}
		if color == "" {
			return
		}
		cs := C.CString(string(color))
		allocated = append(allocated, cs)
		*dst = cs
	}
	set(&settings.rise_color, opts.RiseColor)
	set(&settings.fall_color, opts.FallColor)
	set(&settings.bg_color, opts.BackgroundColor)
	set(&settings.area_color, opts.AreaColor)

	settings.single_color = boolToC(opts.SingleColor)
	settings.show_prices = boolToC(opts.ShowPrices)
	settings.show_times = boolToC(opts.ShowTimes)

	return func() {
		for _, cs := range allocated {
			C.free(unsafe.Pointer(cs))
		}
	}
}

func boolToC(b bool) C.int32_t {
	if b {
		return 1
	}
	return 0
}

// Version reports the version of the underlying C library.
func Version() string { return C.GoString(C.ccharts_version()) }

// MaxDim is the largest width or height in cells (CC_MAX_DIM).
func MaxDim() int { return int(C.ccharts_max_dim()) }

// MaxCells is the largest number of cells in a chart (CC_MAX_CELLS).
func MaxCells() int { return int(C.ccharts_max_cells()) }
