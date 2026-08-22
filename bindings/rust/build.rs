//! Compiles the vendored C ABI into the crate. No system library is involved:
//! ccharts is a single-header library, so the implementation is built from
//! source every time.

fn main() {
    for source in [
        "vendor/ccharts_abi.c",
        "vendor/ccharts_abi.h",
        "vendor/ccharts.h",
    ] {
        println!("cargo:rerun-if-changed={source}");
    }

    let mut build = cc::Build::new();
    build.file("vendor/ccharts_abi.c").include("vendor");

    if build.get_compiler().is_like_msvc() {
        // The block characters are \uXXXX universal character names; without
        // /utf-8 MSVC maps them to the system ANSI codepage and the glyphs it
        // cannot represent become '?' at runtime.
        build.flag("/utf-8");
    } else {
        build.flag("-std=c99");
    }

    build.compile("ccharts_abi");

    // ccharts.h uses lround/sin from libm. Linking it is a no-op on macOS
    // (libSystem) and unnecessary on Windows.
    if cfg!(target_os = "linux") || cfg!(target_os = "freebsd") {
        println!("cargo:rustc-link-lib=m");
    }
}
