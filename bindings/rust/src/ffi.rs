//! Raw declarations for abi/ccharts_abi.h. Kept private; the safe API in
//! lib.rs is the only supported surface.
#![allow(non_camel_case_types)]

use std::ffi::{c_char, c_int, c_longlong};

/// Opaque, immutable dataset handle.
#[repr(C)]
pub struct ccharts_data {
    _private: [u8; 0],
}

#[repr(C)]
pub struct ccharts_settings {
    pub rise_color: *const c_char,
    pub fall_color: *const c_char,
    pub bg_color: *const c_char,
    pub area_color: *const c_char,
    pub single_color: i32,
    pub show_prices: i32,
    pub show_times: i32,
}

#[repr(C)]
pub struct ccharts_pie_slice {
    pub label: *const c_char,
    pub value: f64,
}

#[repr(C)]
pub struct ccharts_hist_settings {
    pub rise_color: *const c_char,
    pub bg_color: *const c_char,
    pub bin_count: i32,
    pub min_value: f64,
    pub max_value: f64,
    pub show_bins: i32,
    pub show_prices: i32,
}

extern "C" {
    pub fn ccharts_from_arrays(
        open: *const f64,
        high: *const f64,
        low: *const f64,
        close: *const f64,
        ts: *const c_longlong,
        n: i32,
        out: *mut *mut ccharts_data,
    ) -> i32;
    pub fn ccharts_parse_json(json: *const c_char, out: *mut *mut ccharts_data) -> i32;
    pub fn ccharts_parse_csv(
        csv: *const c_char,
        value_separator: c_char,
        line_separator: c_char,
        out: *mut *mut ccharts_data,
    ) -> i32;
    pub fn ccharts_data_len(data: *const ccharts_data) -> i32;
    pub fn ccharts_data_free(data: *mut ccharts_data);

    pub fn ccharts_line(
        data: *const ccharts_data,
        width: i32,
        height: i32,
        settings: *const ccharts_settings,
        out: *mut *mut c_char,
        out_len: *mut usize,
    ) -> i32;
    pub fn ccharts_candle(
        data: *const ccharts_data,
        width: i32,
        height: i32,
        settings: *const ccharts_settings,
        out: *mut *mut c_char,
        out_len: *mut usize,
    ) -> i32;
    pub fn ccharts_string_free(s: *mut c_char);

    pub fn ccharts_hist(
        samples: *const f64,
        count: i32,
        width: i32,
        height: i32,
        settings: *const ccharts_hist_settings,
        out: *mut *mut c_char,
        out_len: *mut usize,
    ) -> i32;

    pub fn ccharts_pie_from_slices(
        slices: *const ccharts_pie_slice,
        count: i32,
        width: i32,
        height: i32,
        donut: i32,
        colors: *const *const c_char,
        color_count: i32,
        show_legend: i32,
        show_pct: i32,
        slice_gap: f64,
        inner_radius_ratio: f64,
        legend_format: i32,
        start_angle: f64,
        counter_clockwise: i32,
        center_text: *const c_char,
        out: *mut *mut c_char,
        out_len: *mut usize,
    ) -> i32;

    pub fn ccharts_color(index: c_int) -> *const c_char;
    pub fn ccharts_error_message(status: c_int) -> *const c_char;
    pub fn ccharts_version() -> *const c_char;
    pub fn ccharts_max_dim() -> i32;
    pub fn ccharts_max_cells() -> i32;
}
