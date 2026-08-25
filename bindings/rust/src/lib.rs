//! Financial OHLC data as a string — line and candlestick charts drawn with
//! Unicode block characters, with ANSI color optional, plus pie/donut charts.
//!
//! Nothing is printed for you: [`Chart::line`], [`Chart::candle`] and
//! [`Chart::pie`] return a [`String`], so the chart goes wherever text goes.
//!
//! This crate wraps the C library [`ccharts`](https://github.com/dethrandir/ccharts)
//! through its flat ABI. The C sources are vendored and compiled by
//! `build.rs`, so there is no system dependency and nothing to install.
//!
//! ```
//! use ccharts::{Chart, Color, Settings};
//!
//! let chart = Chart::from_arrays(
//!     &[1.0, 1.5, 1.2],
//!     &[2.0, 2.5, 3.0],
//!     &[0.5, 1.0, 1.1],
//!     &[1.5, 1.2, 2.8],
//!     None,
//! )?;
//!
//! let settings = Settings::new().rise(Color::Blue).show_prices(true);
//! println!("{}", chart.line(60, 8, &settings)?);
//! println!("{}", chart.candle(60, 8, &Settings::new())?);
//! # Ok::<(), ccharts::Error>(())
//! ```
//!
//! Charts are immutable once built and hold no interior mutability, so a
//! [`Chart`] is [`Send`] and [`Sync`] and can be rendered from several threads.

#![deny(missing_docs)]
#![warn(rust_2018_idioms)]

mod ffi;

use std::ffi::{c_char, CStr, CString};
use std::fmt;
use std::ptr::NonNull;

/// Errors reported by the C layer or by argument validation.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Error {
    /// A NULL pointer, an empty dataset, or mismatched column lengths.
    InvalidArgument(&'static str),
    /// The JSON or CSV input could not be parsed.
    Parse,
    /// An allocation failed.
    OutOfMemory,
    /// A price was NaN or infinite.
    NonFinite,
    /// `width`/`height` were not positive or exceeded the library limits.
    Dimensions,
    /// A string argument contained an interior NUL byte.
    InteriorNul,
    /// An unrecognized status code (should not happen).
    Unknown(i32),
}

impl Error {
    fn from_status(status: i32) -> Self {
        match status {
            1 => Error::InvalidArgument("rejected by the C layer"),
            2 => Error::Parse,
            3 => Error::OutOfMemory,
            4 => Error::NonFinite,
            5 => Error::Dimensions,
            other => Error::Unknown(other),
        }
    }

    fn status_message(status: i32) -> &'static str {
        // Safety: ccharts_error_message returns a static string for any input.
        unsafe { CStr::from_ptr(ffi::ccharts_error_message(status)) }
            .to_str()
            .unwrap_or("unknown error")
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Error::InvalidArgument(what) => write!(f, "invalid argument: {what}"),
            Error::Parse => f.write_str(Error::status_message(2)),
            Error::OutOfMemory => f.write_str(Error::status_message(3)),
            Error::NonFinite => f.write_str(Error::status_message(4)),
            Error::Dimensions => f.write_str(Error::status_message(5)),
            Error::InteriorNul => f.write_str("string contains an interior NUL byte"),
            Error::Unknown(code) => write!(f, "unknown ccharts status {code}"),
        }
    }
}

impl std::error::Error for Error {}

type Result<T> = std::result::Result<T, Error>;

/// The sixteen ANSI colors plus the reset sequence.
///
/// The escape sequences come from the C library rather than being duplicated
/// here, so every binding names the same values. For 256-color or truecolor
/// output use [`Settings::rise_ansi`] and friends.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[allow(missing_docs)]
pub enum Color {
    Black,
    Red,
    Green,
    Yellow,
    Blue,
    Magenta,
    Cyan,
    White,
    BrightBlack,
    BrightRed,
    BrightGreen,
    BrightYellow,
    BrightBlue,
    BrightMagenta,
    BrightCyan,
    BrightWhite,
    Reset,
}

impl Color {
    fn index(self) -> i32 {
        self as i32
    }

    /// The ANSI escape sequence for this color.
    pub fn as_str(self) -> &'static str {
        // Safety: the index is always in range, so the pointer is a static
        // NUL-terminated ASCII string.
        unsafe { CStr::from_ptr(ffi::ccharts_color(self.index())) }
            .to_str()
            .unwrap_or("")
    }
}

/// Either a named color or a caller-supplied escape sequence.
#[derive(Debug, Clone)]
enum ColorSpec {
    Named(Color),
    Custom(CString),
}

impl ColorSpec {
    fn as_ptr(&self) -> *const c_char {
        match self {
            // Safety: index is in range for every Color variant.
            ColorSpec::Named(color) => unsafe { ffi::ccharts_color(color.index()) },
            ColorSpec::Custom(s) => s.as_ptr(),
        }
    }
}

/// Rendering options. Every field is optional; unset ones take the library
/// defaults (green rising, red falling, no background, no area fill).
#[derive(Debug, Clone, Default)]
pub struct Settings {
    rise: Option<ColorSpec>,
    fall: Option<ColorSpec>,
    background: Option<ColorSpec>,
    area: Option<ColorSpec>,
    single_color: bool,
    show_prices: bool,
    show_times: bool,
    plain: bool,
}

macro_rules! color_setter {
    ($named:ident, $custom:ident, $field:ident, $doc:expr) => {
        #[doc = $doc]
        pub fn $named(mut self, color: Color) -> Self {
            self.$field = Some(ColorSpec::Named(color));
            self
        }

        #[doc = concat!("Like [`Settings::", stringify!($named), "`], but with a raw ANSI escape sequence (256-color, truecolor, ...). An interior NUL makes the escape be ignored.")]
        pub fn $custom(mut self, escape: &str) -> Self {
            self.$field = CString::new(escape).ok().map(ColorSpec::Custom);
            self
        }
    };
}

impl Settings {
    /// Options with every field at its default.
    pub fn new() -> Self {
        Self::default()
    }

    color_setter!(
        rise,
        rise_ansi,
        rise,
        "Color for rising values and candles."
    );
    color_setter!(
        fall,
        fall_ansi,
        fall,
        "Color for falling values and candles."
    );
    color_setter!(
        background,
        background_ansi,
        background,
        "Background color of empty cells."
    );
    color_setter!(area, area_ansi, area, "Fill color below a line chart.");

    /// Draw the whole chart in one color chosen from the overall change,
    /// instead of coloring each segment by its own direction.
    pub fn single_color(mut self, yes: bool) -> Self {
        self.single_color = yes;
        self
    }

    /// Print max/min price labels in a left margin.
    pub fn show_prices(mut self, yes: bool) -> Self {
        self.show_prices = yes;
        self
    }

    /// Print the first and last timestamp under the chart.
    pub fn show_times(mut self, yes: bool) -> Self {
        self.show_times = yes;
        self
    }

    /// Render with no ANSI escapes at all, overriding every color.
    ///
    /// Use it when the chart is going somewhere that does not interpret
    /// escapes — a log file, an HTML block, a commit message.
    pub fn plain(mut self, yes: bool) -> Self {
        self.plain = yes;
        self
    }

    fn to_raw(&self) -> ffi::ccharts_settings {
        // An empty string tells the C layer to emit no escape at all, which
        // is different from NULL (use the default color).
        const EMPTY: &[u8] = b"\0";
        let plain = EMPTY.as_ptr() as *const c_char;

        let ptr = |spec: &Option<ColorSpec>| -> *const c_char {
            if self.plain {
                plain
            } else {
                spec.as_ref().map_or(std::ptr::null(), ColorSpec::as_ptr)
            }
        };
        ffi::ccharts_settings {
            rise_color: ptr(&self.rise),
            fall_color: ptr(&self.fall),
            bg_color: ptr(&self.background),
            area_color: ptr(&self.area),
            single_color: self.single_color as i32,
            show_prices: self.show_prices as i32,
            show_times: self.show_times as i32,
        }
    }
}

/// One slice of a pie chart: an optional legend label and a positive amount.
///
/// The value is an *amount* — the pie computes the percentage from the sum.
/// A value `<= 0` (zero, negative, NaN, inf) makes the whole render return
/// the empty string rather than an error.
#[derive(Debug, Clone, Copy)]
pub struct PieSlice<'a> {
    /// Legend label; `None` omits it.
    pub label: Option<&'a str>,
    /// A positive amount; the pie computes the percentage.
    pub value: f64,
}

/// Options for [`Chart::pie`]. Every field is optional; unset ones take the
/// library defaults (a filled disk, no override colors, a legend without
/// percentages).
#[derive(Debug, Clone)]
pub struct PieOptions {
    donut: bool,
    colors: Option<Vec<ColorSpec>>,
    show_legend: bool,
    show_pct: bool,
    slice_gap: f64,
    inner_radius_ratio: f64,
    legend_format: i32,
    start_angle: f64,
    counter_clockwise: bool,
    center_text: Option<String>,
}

impl PieOptions {
    /// Options with every field at its default.
    pub fn new() -> Self {
        Self {
            donut: false,
            colors: None,
            show_legend: true,
            show_pct: false,
            slice_gap: 0.0,
            inner_radius_ratio: -1.0,
            legend_format: 0,
            start_angle: -1.0,
            counter_clockwise: false,
            center_text: None,
        }
    }

    /// Hollow out the center (a donut) instead of a filled disk.
    pub fn donut(mut self, yes: bool) -> Self {
        self.donut = yes;
        self
    }

    /// Per-slice override colors from the named palette. Slice `i` uses
    /// `colors[i % colors.len()]`; `None` selects the fixed default palette.
    pub fn colors(mut self, colors: &[Color]) -> Self {
        self.colors = Some(colors.iter().map(|c| ColorSpec::Named(*c)).collect());
        self
    }

    /// Like [`PieOptions::colors`], but with raw ANSI escape sequences
    /// (256-color, truecolor, ...). An interior NUL makes the escape ignored.
    pub fn colors_ansi(mut self, escapes: &[&str]) -> Self {
        self.colors = Some(
            escapes
                .iter()
                .filter_map(|s| CString::new(*s).ok().map(ColorSpec::Custom))
                .collect(),
        );
        self
    }

    /// Print one `label  value (pct%)` line per slice below the disk.
    pub fn show_legend(mut self, yes: bool) -> Self {
        self.show_legend = yes;
        self
    }

    /// Append `(NN%)` to each legend entry.
    pub fn show_pct(mut self, yes: bool) -> Self {
        self.show_pct = yes;
        self
    }

    /// Angular gap between slices, in radians. `0.0` (the default) keeps the
    /// slices adjacent, producing thin gaps when applied to a donut.
    pub fn slice_gap(mut self, radians: f64) -> Self {
        self.slice_gap = radians;
        self
    }

    /// Donut thickness in `[0, 1]`: `0.0` is a filled disk, `1.0` a hairline
    /// ring. A negative value (the default) leaves it to [`PieOptions::donut`]:
    /// `0.5` for a donut, `0.0` for a disk. Values above `1.0` are clamped.
    pub fn inner_radius_ratio(mut self, ratio: f64) -> Self {
        self.inner_radius_ratio = ratio;
        self
    }

    /// Legend entry format: `0` = `label  value` (+ `(NN%)` with
    /// [`PieOptions::show_pct`]), `1` = `label  NN%`, `2` = `value  (NN%)`,
    /// `3` = `label` only. Unknown values fall back to `0`.
    pub fn legend_format(mut self, format: i32) -> Self {
        self.legend_format = format;
        self
    }

    /// Angle (radians) at which slice 0 begins. A negative value (the default)
    /// uses the library default (12 o'clock).
    pub fn start_angle(mut self, radians: f64) -> Self {
        self.start_angle = radians;
        self
    }

    /// Sweep the slices clockwise instead of the default counter-clockwise.
    pub fn counter_clockwise(mut self, yes: bool) -> Self {
        self.counter_clockwise = yes;
        self
    }

    /// Text drawn in the center of a donut (only when there is a hollow).
    /// `None` (the default) disables it.
    pub fn center_text(mut self, text: &str) -> Self {
        self.center_text = Some(text.to_string());
        self
    }
}

impl Default for PieOptions {
    fn default() -> Self {
        Self::new()
    }
}

/// Options for [`Chart::histogram`]. Every field is optional; unset ones
/// take the library defaults (green bars, auto bin count, auto value range
/// from the data min/max, no footer or margin).
///
/// `min_value`/`max_value` are `f64::NAN` by default, which is the "auto"
/// sentinel: the range is taken from the data. Set either to a real number
/// to fix that end of the window.
#[derive(Debug, Clone)]
pub struct HistogramOptions {
    rise: Option<ColorSpec>,
    background: Option<ColorSpec>,
    bin_count: i32,
    min_value: f64,
    max_value: f64,
    show_bins: bool,
    show_prices: bool,
    plain: bool,
}

impl HistogramOptions {
    /// Options with every field at its default.
    pub fn new() -> Self {
        Self {
            rise: None,
            background: None,
            bin_count: 0,
            min_value: f64::NAN,
            max_value: f64::NAN,
            show_bins: false,
            show_prices: false,
            plain: false,
        }
    }

    color_setter!(rise, rise_ansi, rise, "Bar fill color.");
    color_setter!(
        background,
        background_ansi,
        background,
        "Background color of empty cells."
    );

    /// Number of equal-width bins. `0` (the default) selects a value from the
    /// sample count, bounded by the chart width.
    pub fn bin_count(mut self, bins: i32) -> Self {
        self.bin_count = bins;
        self
    }

    /// Lower edge of the value window. `f64::NAN` (the default) uses the data
    /// minimum. Must be strictly less than [`HistogramOptions::max_value`]
    /// when both are set.
    pub fn min_value(mut self, min: f64) -> Self {
        self.min_value = min;
        self
    }

    /// Upper edge of the value window. `f64::NAN` (the default) uses the data
    /// maximum. Must be strictly greater than [`HistogramOptions::min_value`]
    /// when both are set.
    pub fn max_value(mut self, max: f64) -> Self {
        self.max_value = max;
        self
    }

    /// Append a value-axis footer row (window min left, window max right).
    pub fn show_bins(mut self, yes: bool) -> Self {
        self.show_bins = yes;
        self
    }

    /// Print max-count / min-count labels in a left margin.
    pub fn show_prices(mut self, yes: bool) -> Self {
        self.show_prices = yes;
        self
    }

    /// Render with no ANSI escapes at all, overriding every color.
    pub fn plain(mut self, yes: bool) -> Self {
        self.plain = yes;
        self
    }

    fn to_raw(&self) -> ffi::ccharts_hist_settings {
        const EMPTY: &[u8] = b"\0";
        let plain = EMPTY.as_ptr() as *const c_char;
        let ptr = |spec: &Option<ColorSpec>| -> *const c_char {
            if self.plain {
                plain
            } else {
                spec.as_ref().map_or(std::ptr::null(), ColorSpec::as_ptr)
            }
        };
        ffi::ccharts_hist_settings {
            rise_color: ptr(&self.rise),
            bg_color: ptr(&self.background),
            bin_count: self.bin_count,
            min_value: self.min_value,
            max_value: self.max_value,
            show_bins: self.show_bins as i32,
            show_prices: self.show_prices as i32,
        }
    }
}

impl Default for HistogramOptions {
    fn default() -> Self {
        Self::new()
    }
}

/// Options for [`Chart::sparkline`]. Every field is optional; unset ones take
/// the library defaults (green line, no area fill, no reserved edge margins).
#[derive(Debug, Clone)]
pub struct SparklineOptions {
    rise: Option<ColorSpec>,
    area: Option<ColorSpec>,
    min_above: i32,
    min_below: i32,
    plain: bool,
}

impl SparklineOptions {
    /// Options with every field at its default.
    pub fn new() -> Self {
        Self {
            rise: None,
            area: None,
            min_above: 0,
            min_below: 0,
            plain: false,
        }
    }

    color_setter!(rise, rise_ansi, rise, "Trend line color.");
    color_setter!(
        area,
        area_ansi,
        area,
        "Fill color below the line (the area under the trend)."
    );

    /// Reserve this many sub-pixels at the top edge so the line does not clip
    /// at the very top of a tiny chart. Default `0`.
    pub fn min_above(mut self, sub_pixels: i32) -> Self {
        self.min_above = sub_pixels;
        self
    }

    /// Reserve this many sub-pixels at the bottom edge so the line does not
    /// clip at the very bottom of a tiny chart. Default `0`.
    pub fn min_below(mut self, sub_pixels: i32) -> Self {
        self.min_below = sub_pixels;
        self
    }

    /// Render with no ANSI escapes at all, overriding every color.
    pub fn plain(mut self, yes: bool) -> Self {
        self.plain = yes;
        self
    }

    fn to_raw(&self) -> ffi::ccharts_spark_settings {
        const EMPTY: &[u8] = b"\0";
        let plain = EMPTY.as_ptr() as *const c_char;
        let ptr = |spec: &Option<ColorSpec>| -> *const c_char {
            if self.plain {
                plain
            } else {
                spec.as_ref().map_or(std::ptr::null(), ColorSpec::as_ptr)
            }
        };
        ffi::ccharts_spark_settings {
            rise_color: ptr(&self.rise),
            area_color: ptr(&self.area),
            min_above: self.min_above,
            min_below: self.min_below,
        }
    }
}

impl Default for SparklineOptions {
    fn default() -> Self {
        Self::new()
    }
}

/// One bar: a categorical label and a non-negative height.
///
/// The value is a bar height, not a normalized fraction. Negative values are
/// clamped to zero by the renderer rather than drawn below the axis.
#[derive(Debug, Clone, Copy)]
pub struct BarItem<'a> {
    /// Categorical label shown below the column when `show_labels` is set.
    pub label: &'a str,
    /// Bar height; negative values draw at zero height.
    pub value: f64,
}

/// Options for [`Chart::bar`]. Every field is optional; unset ones take the
/// library defaults (green bars, no background, no label or value footer).
#[derive(Debug, Clone)]
pub struct BarOptions {
    rise: Option<ColorSpec>,
    background: Option<ColorSpec>,
    show_labels: bool,
    show_prices: bool,
    plain: bool,
}

impl BarOptions {
    /// Options with every field at its default.
    pub fn new() -> Self {
        Self {
            rise: None,
            background: None,
            show_labels: false,
            show_prices: false,
            plain: false,
        }
    }

    color_setter!(rise, rise_ansi, rise, "Bar fill color.");
    color_setter!(
        background,
        background_ansi,
        background,
        "Background color of empty cells."
    );

    /// Print each column's label in a footer row below the chart.
    pub fn show_labels(mut self, yes: bool) -> Self {
        self.show_labels = yes;
        self
    }

    /// Print the max bar value and 0 (the baseline) in a left value-axis
    /// margin.
    pub fn show_prices(mut self, yes: bool) -> Self {
        self.show_prices = yes;
        self
    }

    /// Render with no ANSI escapes at all, overriding every color.
    pub fn plain(mut self, yes: bool) -> Self {
        self.plain = yes;
        self
    }

    fn to_raw(&self) -> ffi::ccharts_bar_settings {
        const EMPTY: &[u8] = b"\0";
        let plain = EMPTY.as_ptr() as *const c_char;
        let ptr = |spec: &Option<ColorSpec>| -> *const c_char {
            if self.plain {
                plain
            } else {
                spec.as_ref().map_or(std::ptr::null(), ColorSpec::as_ptr)
            }
        };
        ffi::ccharts_bar_settings {
            rise_color: ptr(&self.rise),
            bg_color: ptr(&self.background),
            show_labels: self.show_labels as i32,
            show_prices: self.show_prices as i32,
        }
    }
}

impl Default for BarOptions {
    fn default() -> Self {
        Self::new()
    }
}

/// Options for [`Chart::stacked_bar`]. Every field is optional; unset ones
/// take the library defaults (the fixed deterministic per-series palette, no
/// background, no category-label or value footer).
#[derive(Debug, Clone)]
pub struct StackOptions {
    colors: Option<Vec<ColorSpec>>,
    background: Option<ColorSpec>,
    category_labels: Option<Vec<String>>,
    show_labels: bool,
    show_prices: bool,
    plain: bool,
}

impl StackOptions {
    /// Options with every field at its default.
    pub fn new() -> Self {
        Self {
            colors: None,
            background: None,
            category_labels: None,
            show_labels: false,
            show_prices: false,
            plain: false,
        }
    }

    /// Per-series override colors from the named palette. Series `i` uses
    /// `colors[i % colors.len()]`; `None` selects the fixed default palette.
    pub fn colors(mut self, colors: &[Color]) -> Self {
        self.colors = Some(colors.iter().map(|c| ColorSpec::Named(*c)).collect());
        self
    }

    /// Like [`StackOptions::colors`], but with raw ANSI escape sequences
    /// (256-color, truecolor, ...). An interior NUL makes the escape ignored.
    pub fn colors_ansi(mut self, escapes: &[&str]) -> Self {
        self.colors = Some(
            escapes
                .iter()
                .filter_map(|s| CString::new(*s).ok().map(ColorSpec::Custom))
                .collect(),
        );
        self
    }

    color_setter!(
        background,
        background_ansi,
        background,
        "Background color of empty cells above the tallest stack."
    );

    /// Category names printed, when [`StackOptions::show_labels`] is set, one
    /// per output column in the footer. An interior NUL in any label is
    /// rejected when rendering.
    pub fn category_labels(mut self, labels: &[&str]) -> Self {
        self.category_labels = Some(labels.iter().map(|s| s.to_string()).collect());
        self
    }

    /// Print each column's category label in a footer row below the chart.
    pub fn show_labels(mut self, yes: bool) -> Self {
        self.show_labels = yes;
        self
    }

    /// Print the tallest stack total and 0 (the baseline) in a left
    /// value-axis margin.
    pub fn show_prices(mut self, yes: bool) -> Self {
        self.show_prices = yes;
        self
    }

    /// Render with no ANSI escapes at all, overriding every color.
    pub fn plain(mut self, yes: bool) -> Self {
        self.plain = yes;
        self
    }
}

impl Default for StackOptions {
    fn default() -> Self {
        Self::new()
    }
}

/// A parsed OHLC dataset that can be rendered as a line or candle chart.
pub struct Chart {
    handle: NonNull<ffi::ccharts_data>,
}

// Safety: the handle points at an immutable dataset, the C library keeps no
// mutable global state, and the rendering functions only read from it.
unsafe impl Send for Chart {}
unsafe impl Sync for Chart {}

impl Chart {
    /// Builds a chart from four equal-length price columns and optional epoch
    /// seconds. The data is copied, so the slices are not borrowed afterwards.
    pub fn from_arrays(
        open: &[f64],
        high: &[f64],
        low: &[f64],
        close: &[f64],
        ts: Option<&[i64]>,
    ) -> Result<Self> {
        let n = open.len();
        if n == 0 {
            return Err(Error::InvalidArgument("need at least one candle"));
        }
        if high.len() != n || low.len() != n || close.len() != n {
            return Err(Error::InvalidArgument(
                "open, high, low and close must have the same length",
            ));
        }
        if ts.is_some_and(|t| t.len() != n) {
            return Err(Error::InvalidArgument(
                "ts must have the same length as the price columns",
            ));
        }
        let count = i32::try_from(n).map_err(|_| Error::InvalidArgument("too many candles"))?;

        let mut out = std::ptr::null_mut();
        // Safety: every pointer is valid for `count` elements and the C layer
        // copies immediately without retaining any of them.
        let status = unsafe {
            ffi::ccharts_from_arrays(
                open.as_ptr(),
                high.as_ptr(),
                low.as_ptr(),
                close.as_ptr(),
                ts.map_or(std::ptr::null(), |t| t.as_ptr()),
                count,
                &mut out,
            )
        };
        Self::wrap(status, out)
    }

    /// Builds a chart from the fixed-schema JSON document described in the
    /// project README: an array of objects with `ts`, `open`, `high`, `low`
    /// and `close`.
    pub fn from_json(json: &str) -> Result<Self> {
        let json = CString::new(json).map_err(|_| Error::InteriorNul)?;
        let mut out = std::ptr::null_mut();
        // Safety: `json` is a valid NUL-terminated string for this call.
        let status = unsafe { ffi::ccharts_parse_json(json.as_ptr(), &mut out) };
        Self::wrap(status, out)
    }

    /// Builds a chart from CSV rows of `open,high,low,close[,timestamp]`.
    /// Blank lines are skipped.
    pub fn from_csv(csv: &str, value_separator: u8, line_separator: u8) -> Result<Self> {
        let csv = CString::new(csv).map_err(|_| Error::InteriorNul)?;
        if value_separator == 0 || line_separator == 0 {
            return Err(Error::InvalidArgument("separators must not be NUL"));
        }
        let mut out = std::ptr::null_mut();
        // Safety: `csv` is a valid NUL-terminated string for this call.
        let status = unsafe {
            ffi::ccharts_parse_csv(
                csv.as_ptr(),
                value_separator as c_char,
                line_separator as c_char,
                &mut out,
            )
        };
        Self::wrap(status, out)
    }

    /// Renders a pie or donut chart from the given slices.
    ///
    /// A pie has no OHLC data, so this is an associated function rather than a
    /// method: it takes the slices directly. `options.colors` override the
    /// per-slice palette when set; all-zero or non-positive slice values
    /// produce the empty string, while NaN/inf are rejected with
    /// [`Error::NonFinite`].
    pub fn pie(slices: &[PieSlice<'_>], width: u32, height: u32, options: &PieOptions) -> Result<String> {
        if slices.is_empty() {
            return Err(Error::InvalidArgument("need at least one slice"));
        }
        let count = i32::try_from(slices.len())
            .map_err(|_| Error::InvalidArgument("too many slices"))?;
        let (width, height) = match (i32::try_from(width), i32::try_from(height)) {
            (Ok(w), Ok(h)) => (w, h),
            _ => return Err(Error::Dimensions),
        };

        // The labels must outlive the call, so the CStrings are kept in a
        // parallel vector alongside the raw slice array.
        let mut labels: Vec<Option<CString>> = Vec::with_capacity(slices.len());
        let mut raw: Vec<ffi::ccharts_pie_slice> = Vec::with_capacity(slices.len());
        for slice in slices {
            let label = match slice.label {
                Some(text) => Some(CString::new(text).map_err(|_| Error::InteriorNul)?),
                None => None,
            };
            raw.push(ffi::ccharts_pie_slice {
                label: label.as_ref().map_or(std::ptr::null(), |c| c.as_ptr()),
                value: slice.value,
            });
            labels.push(label);
        }

        let colors: Vec<*const c_char> = options
            .colors
            .as_ref()
            .map(|cs| cs.iter().map(ColorSpec::as_ptr).collect())
            .unwrap_or_default();
        let colors_ptr = if colors.is_empty() {
            std::ptr::null()
        } else {
            colors.as_ptr()
        };

        let mut out: *mut c_char = std::ptr::null_mut();
        let mut len: usize = 0;
        // The center-text pointer must outlive the call.
        let center_text = options
            .center_text
            .as_ref()
            .map(|text| CString::new(text.as_str()).map_err(|_| Error::InteriorNul))
            .transpose()?;
        let center_text_ptr = center_text
            .as_ref()
            .map_or(std::ptr::null(), |c| c.as_ptr());
        // Safety: every pointer in `raw`/`colors`/`center_text_ptr` lives across
        // the call, the C layer copies the slices immediately, and `out`
        // receives an owned string we release below.
        let status = unsafe {
            ffi::ccharts_pie_from_slices(
                raw.as_ptr(),
                count,
                width,
                height,
                options.donut as i32,
                colors_ptr,
                colors.len() as i32,
                options.show_legend as i32,
                options.show_pct as i32,
                options.slice_gap,
                options.inner_radius_ratio,
                options.legend_format,
                options.start_angle,
                options.counter_clockwise as i32,
                center_text_ptr,
                &mut out,
                &mut len,
            )
        };
        if status != 0 {
            return Err(Error::from_status(status));
        }
        // Safety: `out` is a library-owned buffer released by take_string.
        unsafe { Self::take_string(out, len) }
    }

    /// Renders a histogram of the given scalar samples.
    ///
    /// A histogram has no OHLC data, so (like [`Chart::pie`]) this is an
    /// associated function: it takes the samples directly. `options.min_value`
    /// / `options.max_value` fix the window when set (each defaults to `NaN`
    /// = auto from the data), `options.bin_count` fixes the number of bins
    /// (0 = auto). NaN or infinite samples are rejected with
    /// [`Error::NonFinite`].
    pub fn histogram(
        samples: &[f64],
        width: u32,
        height: u32,
        options: &HistogramOptions,
    ) -> Result<String> {
        if samples.is_empty() {
            return Err(Error::InvalidArgument("need at least one sample"));
        }
        let count = i32::try_from(samples.len())
            .map_err(|_| Error::InvalidArgument("too many samples"))?;
        let (width, height) = match (i32::try_from(width), i32::try_from(height)) {
            (Ok(w), Ok(h)) => (w, h),
            _ => return Err(Error::Dimensions),
        };

        let raw = options.to_raw();
        let mut out: *mut c_char = std::ptr::null_mut();
        let mut len: usize = 0;
        // Safety: the samples and settings live across the call, and `out`
        // receives an owned string we release below.
        let status = unsafe {
            ffi::ccharts_hist(
                samples.as_ptr(),
                count,
                width,
                height,
                &raw,
                &mut out,
                &mut len,
            )
        };
        if status != 0 {
            return Err(Error::from_status(status));
        }
        // Safety: `out` is a library-owned buffer released by take_string.
        unsafe { Self::take_string(out, len) }
    }

    /// Renders a sparkline of the given scalar samples.
    ///
    /// A sparkline has no OHLC data, so (like [`Chart::pie`]) this is an
    /// associated function: it takes the samples directly. `options.rise`
    /// / `options.area` override the line/fill colors, and `options.min_above`
    /// / `options.min_below` reserve sub-pixels at the top/bottom edge so the
    /// line does not clip. NaN or infinite samples are rejected with
    /// [`Error::NonFinite`].
    pub fn sparkline(
        samples: &[f64],
        width: u32,
        height: u32,
        options: &SparklineOptions,
    ) -> Result<String> {
        if samples.is_empty() {
            return Err(Error::InvalidArgument("need at least one sample"));
        }
        let count = i32::try_from(samples.len())
            .map_err(|_| Error::InvalidArgument("too many samples"))?;
        let (width, height) = match (i32::try_from(width), i32::try_from(height)) {
            (Ok(w), Ok(h)) => (w, h),
            _ => return Err(Error::Dimensions),
        };

        let raw = options.to_raw();
        let mut out: *mut c_char = std::ptr::null_mut();
        let mut len: usize = 0;
        // Safety: the samples and settings live across the call, and `out`
        // receives an owned string we release below.
        let status = unsafe {
            ffi::ccharts_spark(
                samples.as_ptr(),
                count,
                width,
                height,
                &raw,
                &mut out,
                &mut len,
            )
        };
        if status != 0 {
            return Err(Error::from_status(status));
        }
        // Safety: `out` is a library-owned buffer released by take_string.
        unsafe { Self::take_string(out, len) }
    }

    /// Renders a categorical bar chart of the `(label, value)` pairs formed
    /// by the parallel `labels` and `values` arrays.
    ///
    /// A bar chart has no OHLC data, so (like [`Chart::pie`]) this is an
    /// associated function: it takes the labels and values directly. The two
    /// arrays must have the same length. `options.rise`/`options.background`
    /// override the bar and background colors, `options.show_labels` prints a
    /// label footer, and `options.show_prices` prints a value axis. Negative
    /// values are clamped to zero by the renderer, while NaN or infinite
    /// values are rejected with [`Error::NonFinite`].
    pub fn bar(
        labels: &[&str],
        values: &[f64],
        width: u32,
        height: u32,
        options: &BarOptions,
    ) -> Result<String> {
        if labels.is_empty() {
            return Err(Error::InvalidArgument("need at least one bar"));
        }
        if labels.len() != values.len() {
            return Err(Error::InvalidArgument(
                "labels and values must have the same length",
            ));
        }
        let count = i32::try_from(labels.len())
            .map_err(|_| Error::InvalidArgument("too many bars"))?;
        let (width, height) = match (i32::try_from(width), i32::try_from(height)) {
            (Ok(w), Ok(h)) => (w, h),
            _ => return Err(Error::Dimensions),
        };

        // The labels must outlive the call, so the CStrings are kept in a
        // parallel vector alongside the raw item array.
        let mut cstr_labels: Vec<CString> = Vec::with_capacity(labels.len());
        let mut raw: Vec<ffi::ccharts_bar_slice> = Vec::with_capacity(labels.len());
        for (label, value) in labels.iter().zip(values.iter()) {
            let cstr = CString::new(*label).map_err(|_| Error::InteriorNul)?;
            raw.push(ffi::ccharts_bar_slice {
                label: cstr.as_ptr(),
                value: *value,
            });
            cstr_labels.push(cstr);
        }

        let raw_settings = options.to_raw();
        let mut out: *mut c_char = std::ptr::null_mut();
        let mut len: usize = 0;
        // Safety: every pointer in `raw` and the settings live across the
        // call, the C layer copies the items immediately, and `out` receives
        // an owned string we release below.
        let status = unsafe {
            ffi::ccharts_bar(
                raw.as_ptr(),
                count,
                width,
                height,
                &raw_settings,
                &mut out,
                &mut len,
            )
        };
        if status != 0 {
            return Err(Error::from_status(status));
        }
        // Safety: `out` is a library-owned buffer released by take_string.
        unsafe { Self::take_string(out, len) }
    }

    /// Renders a stacked bar chart of `(name, values)` series. Every series
    /// carries one value per category; all series must share the same number
    /// of values. Each category's bar is the vertical SUM of its series'
    /// values, drawn as stacked segments.
    ///
    /// A stacked bar has no OHLC data, so (like [`Chart::bar`]) this is an
    /// associated function: it takes the series directly. `options.colors`
    /// override the per-series palette, `options.category_labels` names each
    /// output column in the footer when `options.show_labels` is set, and
    /// `options.show_prices` prints a value axis. Negative values are clamped
    /// to zero by the renderer, while NaN or infinite values are rejected
    /// with [`Error::NonFinite`].
    pub fn stacked_bar(
        series: &[(&str, &[f64])],
        width: u32,
        height: u32,
        options: &StackOptions,
    ) -> Result<String> {
        if series.is_empty() {
            return Err(Error::InvalidArgument("need at least one series"));
        }
        let cats = series[0].1.len();
        if cats == 0 {
            return Err(Error::InvalidArgument("series values must not be empty"));
        }
        if series.iter().any(|(_, v)| v.len() != cats) {
            return Err(Error::InvalidArgument(
                "all series must have the same number of values",
            ));
        }
        let series_count = i32::try_from(series.len())
            .map_err(|_| Error::InvalidArgument("too many series"))?;
        let cat_count = i32::try_from(cats)
            .map_err(|_| Error::InvalidArgument("too many categories"))?;
        let (width, height) = match (i32::try_from(width), i32::try_from(height)) {
            (Ok(w), Ok(h)) => (w, h),
            _ => return Err(Error::Dimensions),
        };

        // The series names (CStrings) and the values slices (through the
        // pointer in each raw series) must outlive the call: the C layer
        // copies the series structs immediately but reads most of them
        // through the pointers.
        let mut names: Vec<CString> = Vec::with_capacity(series.len());
        let mut raw: Vec<ffi::ccharts_stack_series> = Vec::with_capacity(series.len());
        for (name, values) in series {
            let cstr = CString::new(*name).map_err(|_| Error::InteriorNul)?;
            raw.push(ffi::ccharts_stack_series {
                name: cstr.as_ptr(),
                values: values.as_ptr(),
            });
            names.push(cstr);
        }

        // Colors: a NULL-terminated per-series palette. `plain` substitutes
        // one empty escape per series so the render has no ANSI codes at all.
        const EMPTY: &[u8] = b"\0";
        let empty = EMPTY.as_ptr() as *const c_char;
        let mut color_ptrs: Vec<*const c_char> = Vec::new();
        if options.plain {
            for _ in 0..series.len() {
                color_ptrs.push(empty);
            }
        } else if let Some(specs) = &options.colors {
            color_ptrs.extend(specs.iter().map(ColorSpec::as_ptr));
        }
        color_ptrs.push(std::ptr::null());
        let colors_ptr = if color_ptrs.len() > 1 {
            color_ptrs.as_ptr()
        } else {
            std::ptr::null()
        };

        let bg_color = if options.plain {
            empty
        } else {
            options
                .background
                .as_ref()
                .map_or(std::ptr::null(), ColorSpec::as_ptr)
        };

        // Category labels: a `cats`-length array (or NULL when absent).
        // Converted here so an interior NUL can be rejected.
        let cat_cstrs: Vec<CString> = options
            .category_labels
            .as_deref()
            .map(|labels| {
                labels
                    .iter()
                    .map(|l| CString::new(l.as_str()).map_err(|_| Error::InteriorNul))
                    .collect::<Result<Vec<_>>>()
            })
            .transpose()?
            .unwrap_or_default();
        let cat_ptrs: Vec<*const c_char> = cat_cstrs.iter().map(|c| c.as_ptr()).collect();
        let cat_labels_ptr = if cat_ptrs.is_empty() {
            std::ptr::null()
        } else {
            cat_ptrs.as_ptr()
        };

        let settings = ffi::ccharts_stack_settings {
            colors: colors_ptr,
            bg_color,
            cat_labels: cat_labels_ptr,
            series: series_count,
            cats: cat_count,
            show_labels: options.show_labels as i32,
            show_prices: options.show_prices as i32,
        };

        let mut out: *mut c_char = std::ptr::null_mut();
        let mut len: usize = 0;
        // Safety: every pointer in `raw`, `color_ptrs`, `cat_ptrs` and the
        // settings live across the call, the C layer copies the series
        // structs immediately, and `out` receives an owned string we release
        // below.
        let status = unsafe {
            ffi::ccharts_stack(
                raw.as_ptr(),
                series_count,
                width,
                height,
                &settings,
                &mut out,
                &mut len,
            )
        };
        if status != 0 {
            return Err(Error::from_status(status));
        }
        // Safety: `out` is a library-owned buffer released by take_string.
        unsafe { Self::take_string(out, len) }
    }

    fn wrap(status: i32, out: *mut ffi::ccharts_data) -> Result<Self> {
        match NonNull::new(out) {
            Some(handle) if status == 0 => Ok(Chart { handle }),
            _ => Err(Error::from_status(if status == 0 { 3 } else { status })),
        }
    }

    /// Number of candles in the dataset.
    pub fn len(&self) -> usize {
        // Safety: the handle is valid for the lifetime of `self`.
        unsafe { ffi::ccharts_data_len(self.handle.as_ptr()) as usize }
    }

    /// Always false — a chart cannot be built from an empty dataset.
    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// Renders a line chart of the closing prices.
    pub fn line(&self, width: u32, height: u32, settings: &Settings) -> Result<String> {
        self.render(ffi::ccharts_line, width, height, settings)
    }

    /// Renders a candlestick chart.
    pub fn candle(&self, width: u32, height: u32, settings: &Settings) -> Result<String> {
        self.render(ffi::ccharts_candle, width, height, settings)
    }

    fn render(
        &self,
        draw: unsafe extern "C" fn(
            *const ffi::ccharts_data,
            i32,
            i32,
            *const ffi::ccharts_settings,
            *mut *mut c_char,
            *mut usize,
        ) -> i32,
        width: u32,
        height: u32,
        settings: &Settings,
    ) -> Result<String> {
        let (width, height) = match (i32::try_from(width), i32::try_from(height)) {
            (Ok(w), Ok(h)) => (w, h),
            _ => return Err(Error::Dimensions),
        };

        let raw = settings.to_raw();
        let mut out: *mut c_char = std::ptr::null_mut();
        let mut len: usize = 0;
        // Safety: the handle and the settings live across the call, and `out`
        // receives an owned string we release below.
        let status = unsafe {
            draw(
                self.handle.as_ptr(),
                width,
                height,
                &raw,
                &mut out,
                &mut len,
            )
        };
        if status != 0 {
            return Err(Error::from_status(status));
        }

        // Safety: `out` receives a library-owned buffer released here.
        unsafe { Self::take_string(out, len) }
    }

    /// Copies a library-returned string out of C memory and releases it.
    ///
    /// # Safety
    /// `out` must point at a NUL-terminated buffer of at least `len` bytes
    /// owned by the C layer (released with `ccharts_string_free`).
    unsafe fn take_string(out: *mut c_char, len: usize) -> Result<String> {
        if out.is_null() {
            return Err(Error::OutOfMemory);
        }

        // Safety: `out` points at `len` bytes of UTF-8 produced by the library
        // (ASCII plus the block characters), owned by us until we free it.
        let chart = unsafe { std::slice::from_raw_parts(out as *const u8, len) };
        let owned = String::from_utf8_lossy(chart).into_owned();
        unsafe { ffi::ccharts_string_free(out) };
        Ok(owned)
    }
}

impl Drop for Chart {
    fn drop(&mut self) {
        // Safety: the handle was produced by the C layer and is dropped once.
        unsafe { ffi::ccharts_data_free(self.handle.as_ptr()) };
    }
}

impl fmt::Debug for Chart {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Chart")
            .field("candles", &self.len())
            .finish()
    }
}

/// Version of the underlying C library.
pub fn version() -> &'static str {
    // Safety: ccharts_version returns a static NUL-terminated string.
    unsafe { CStr::from_ptr(ffi::ccharts_version()) }
        .to_str()
        .unwrap_or("unknown")
}

/// Maximum width or height in cells (`CC_MAX_DIM`).
pub fn max_dim() -> u32 {
    // Safety: no arguments, returns a constant.
    unsafe { ffi::ccharts_max_dim() as u32 }
}

/// Maximum number of cells in a chart (`CC_MAX_CELLS`).
pub fn max_cells() -> u32 {
    // Safety: no arguments, returns a constant.
    unsafe { ffi::ccharts_max_cells() as u32 }
}
