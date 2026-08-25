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

// setColors fills the C settings struct and returns a function releasing the
// C strings it allocated.
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
