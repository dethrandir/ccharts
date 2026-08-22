//! Terminal charts for financial OHLC data — line and candlestick charts
//! rendered as ANSI-colored strings of Unicode block characters.
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

    fn to_raw(&self) -> ffi::ccharts_settings {
        fn ptr(spec: &Option<ColorSpec>) -> *const c_char {
            spec.as_ref().map_or(std::ptr::null(), ColorSpec::as_ptr)
        }
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
