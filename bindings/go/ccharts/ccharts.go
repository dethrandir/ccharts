// Package ccharts renders financial OHLC data as terminal charts: line and
// candlestick charts returned as ANSI-colored strings of Unicode block
// characters, ready to print.
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

// setColors fills the C settings struct and returns a function releasing the
// C strings it allocated.
func setColors(settings *C.ccharts_settings, opts *Options) func() {
	if opts == nil {
		return func() {}
	}

	var allocated []*C.char
	set := func(dst **C.char, color Color) {
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
