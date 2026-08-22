# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

`ccharts` is a single-header C library (`ccharts.h`) that renders financial OHLC
data as ANSI-colored terminal charts, plus a thin CPython extension that exposes
it to Python. The C header is the only place chart logic lives;
`ccharts/wrapper.c` and `ccharts/__init__.py` only marshal arguments and manage
memory, and `ccharts/_pandas.py` only reshapes DataFrames into columns.

## Commands

```sh
make test          # compiles main.c (gcc -Wall -g -lm) AND runs the demo on prices.txt
make test-py       # setup.py build_ext --inplace, then the unittest suite
make clean         # rm build/, ccharts/*.so, __pycache__

python3 -m unittest discover -s tests -v                                      # all Python tests
python3 -m unittest tests.test_python_api.TestTimezones -v                    # one class
python3 -m unittest tests.test_python_api.TestCCharts.test_candle_uses_wick   # one test

python3 -m pip install -e ".[pandas]"              # editable install (what CI uses)
python3 -m pip wheel . --no-deps -w /tmp/cc_wheel  # wheel build smoke test
```

The Python targets need `setuptools` installed for the interpreter in use (the
system `python3` here does not have it — use a venv).

CI mirrors these and additionally enforces a **warning-free** compile; keep it
that way:

```sh
gcc -Wall -c ccharts/wrapper.c $(python3-config --includes) -o /tmp/wrapper.o
gcc -Wall main.c -lm -o /tmp/ccharts_demo
gcc -Wall -Wextra -Werror -std=c99 -pedantic -I. -Iabi -c abi/ccharts_abi.c -o /tmp/abi.o
```

The C ABI and the cross-language conformance suite:

```sh
cmake -S . -B build && cmake --build build   # shared + static + smoke test
ctest --test-dir build --output-on-failure
python3 scripts/gen_golden.py                # regenerate conformance/golden/
python3 scripts/gen_golden.py --check        # CI: fail on any render drift
python3 scripts/sync_sources.py --check      # CI: vendored copies in sync
python3 scripts/check_versions.py            # CI: one version everywhere
python3 scripts/sync_sources.py              # refresh the bindings' C copies
```

The bindings:

```sh
cd bindings/rust && cargo test          # 13 API tests + the golden suite
cd bindings/rust && cargo clippy --all-targets -- -D warnings
cd bindings/go && go test ./...         # API tests + the golden suite
cd bindings/go && go test -run TestConformance -v ./...   # one golden case each
cd bindings/js && node --test            # API tests + the golden suite
cd bindings/js && EMCC=/path/to/emcc npm run build   # rebuild the .wasm

# These two need the CMake build first (cmake --build build):
CCHARTS_NATIVE_DIR=$PWD/build dotnet test bindings/dotnet/tests/Ccharts.Tests
cd bindings/java && CCHARTS_NATIVE_DIR=$PWD/../../build mvn test
```

## Architecture

**Layering.** `ccharts.h` → `ccharts/wrapper.c` (module `ccharts._core`, four
functions: `parse_json`, `parse_arrays`, `create_line`, `create_candle`) →
`ccharts/__init__.py` (class `Chart` with `.line()` / `.candle()`). Behavior
changes belong in the header; the other two layers should stay mechanical.
`main.c` is a C demo that exercises the same path against `prices.txt`.

**Two input paths, one capsule.** `parse_json` (a JSON document) and
`parse_arrays` (four float64 columns + optional int64 epoch seconds) both
produce the same `"ccharts.ohlc"` PyCapsule, so everything downstream is
shared. `parse_arrays` reads each column through the buffer protocol when it
is a 1-D C-contiguous native block and falls back to `PySequence_Fast`
otherwise — that is what makes `Chart.from_arrays` accept lists, numpy arrays
and `array.array` alike. `Chart.from_dataframe` is a thin pandas adapter on
top of it (`ccharts/_pandas.py`), so any regression test for the columnar
path should live in `tests/test_python_api.py` (no pandas needed) rather than
`tests/test_pandas_api.py`.

**NaN and inf must never reach the renderer.** `cc_pixel` feeds values to
`lround()`, which is undefined for non-finite input. `check_finite` in
`wrapper.c` rejects them (`ValueError`); `_pandas.py` drops those rows before
that (`dropna=True`, the default). The check lives in `wrapper.c`, not
`ccharts.h`, so the header keeps its C89-compatible surface.

**The C ABI layer (`abi/`).** `ccharts.h` exports no symbols (`CC_INLINE` is
`static inline`) and `struct cc_ohlc` is private to the implementation block,
so nothing in it is reachable by an FFI. `abi/ccharts_abi.c` is the one
translation unit that instantiates the header and re-exports a flat
`extern "C"` API (opaque `ccharts_data*`, `ccharts_status` return codes,
caller-frees strings). Go/Rust/C#/Java/WASM bindings all target this, never
the header directly. Three deliberate divergences from the raw header, each
because the raw behavior does not survive an FFI boundary: invalid dimensions
are `CCHARTS_ERR_DIMENSIONS` rather than an empty string, non-finite prices are
rejected, and `ccharts_parse_csv` pre-counts rows because `cc_str_to_ohlc`
never reports how many it actually parsed. Build with CMake; only the 13
`ccharts_*` symbols are exported (visibility is hidden by default and CI
asserts the count).

**`_POSIX_C_SOURCE` is set in `abi/ccharts_abi.c` on purpose.** `ccharts.h`
calls `gmtime_r`, which is POSIX rather than ISO C, so glibc hides it under
`-std=c99 -pedantic` — the dialect Rust's `cc` crate and several package builds
default to. Defining the feature macro in the ABI keeps that flag out of every
downstream build.

**Conformance is what keeps the bindings honest.** `conformance/cases.json`
declares 20 cases (both charts × downsampling × labels × timezones × flat range
× single candle × JSON and array entry points);
`conformance/golden/<case>.txt` holds the expected raw bytes, generated from
the ABI by `scripts/gen_golden.py` (ctypes against the built shared library, so
it also tests the export surface). Every binding must reproduce them byte for
byte — the Python one does in `tests/test_conformance.py`. Two CI guards keep
this from rotting: `gen_golden.py --check` fails on any unintended render
change, and `sync_sources.py --check` fails when a binding's vendored copy of
`ccharts.h`/the ABI drifts from the original (Go needs cgo sources inside the
package directory, and `cargo package` only ships crate-local files, so the
copies are unavoidable). `scripts/check_versions.py` does the same for the
version string across manifests.

**Bindings (`bindings/`).** Rust (`bindings/rust`, crate `ccharts`) and Go
(`bindings/go`, module `github.com/dethrandir/ccharts/bindings/go`) both
compile the vendored ABI from source — the `cc` crate and cgo respectively —
so neither needs a prebuilt library and neither uses the CMake build. Both
expose the same shape as the Python API (`from_arrays`/`from_json`/`from_csv`,
then `line`/`candle`), both hold the dataset immutably and are safe to share
across threads, and both run the shared conformance suite. Two constraints
worth knowing before editing them: the Go module lives in a subdirectory, so
its tags must be `bindings/go/vX.Y.Z`; and `cargo package` only ships
crate-local files, which is why `bindings/rust/tests/conformance.rs` is
excluded from the published crate while `tests/api.rs` is included.

**The JavaScript binding is WebAssembly (`bindings/js`).** `scripts/build.sh`
compiles the vendored ABI with emscripten (`-sSTANDALONE_WASM`) and embeds the
result in `src/wasm-binary.js` as base64, so the npm package is dependency-free
and identical on every platform — no prebuilt binaries, no fetch, no bundler
config. Three things to know before touching it: the module imports five
`wasi_snapshot_preview1` stdio functions plus `env.emscripten_notify_memory_growth`
(pulled in by `snprintf`/`fprintf`, never called in practice) and `src/index.js`
stubs them; exports carry no leading underscore in standalone mode
(`exports.malloc`, not `exports._malloc`) and `_initialize()` must run once
before the heap is touched; and memory growth detaches views, so every helper
re-reads `exports.memory.buffer` instead of caching a view. `src/wasm-binary.js`
is checked in — CI needs only Node — and the conformance test is what catches a
stale one: change the C and regenerate the goldens, and the JS suite fails until
`npm run build` (which needs emscripten) is re-run.

**C# and Java link the library; they do not compile it.** Unlike Rust, Go and
the WASM build, `bindings/dotnet` (source-generated P/Invoke, net8.0) and
`bindings/java` (FFM, JDK 22+, zero JNI code) consume the shared library the
root CMake build produces. Both therefore need prebuilt natives to ship —
`runtimes/{rid}/native/` in the NuGet package, `native/{os}-{arch}/` in the jar
— and both fall back, when those are absent, to `CCHARTS_NATIVE_DIR` and then
to probing for `build/` above the assembly or working directory, which is how
their tests run in-repo. That CI matrix which fills those directories is the
remaining work before either can be published; the projects already read from
them. Two mechanics that cost time to find: in Java, `invokeExact` matches the
argument's *static* type, so a conditional expression passed straight to it is
typed `Object` and throws `WrongMethodTypeException` — the optional timestamp
column goes through a typed local; and the .NET test project sets
`RollForward` because the library targets net8.0 while a machine may only have
a newer runtime.

**Single-header discipline.** `CCHARTS_IMPLEMENTATION` must be defined in exactly
one translation unit — currently `main.c` and `wrapper.c`, which are separate
builds. Everything under that `#ifdef` is private; declarations, the color/block
macros and `CC_MAX_DIM`/`CC_MAX_CELLS` sit above it so clients can use them.

**Render pipeline** (both chart types, in `ccharts.h`):

1. Parse into `cc_ohlc_t[]` — `cc_str_to_ohlc` (CSV) or `cc_json_to_ohlc`
   (fixed-schema JSON, iterative mini-scanner that matches keys exactly).
2. Map data to output columns. When `width < size`, columns downsample: the line
   chart averages the closes in a column, the candle chart aggregates via
   `cc_agg_ohlc` (first open, max high, min low, last close). When
   `width >= size`, candles get several columns each with the last one left as a
   gap.
3. Value → pixel row via `cc_pixel`. **Sub-cell resolution differs per chart:**
   line charts are 8 pixels per cell (`cc_lower_eighth`, `▁▂▃▄▅▆▇█`), candles are
   2 pixels per cell (`▀▄█`) with body/wick tracked as bitmasks (bit0 = lower
   half, bit1 = upper half) and rendered by `cc_render_cell`; body wins over wick.
4. Every cell becomes a **fixed 32-byte string slot** in the `columns` buffer,
   indexed `columns[(x * height + y) * 32]`, with `y` counted from the bottom.
   Any new cell content (color + glyph + `CC_COLOR_RESET`) must fit in 32 bytes
   including the NUL — this budget is what keeps `cc_assemble_chart`'s allocation
   math correct.
5. `cc_assemble_chart` joins rows top-to-bottom, optionally prepending an 8-column
   price margin (max on top row, min on bottom) and appending a timestamp footer
   whose `strftime` format `cc_time_format` picks from the average candle interval.

**Settings.** `cc_settings_t` is meant to be brace-initialized partially;
`cc_settings_resolve` fills unset fields (green rise / red fall, no bg, no area
fill). Never read a settings field without going through it.

**pandas is optional and lazily imported.** `ccharts/_pandas.py` is imported
inside `Chart.from_dataframe`, never at module level — the wheel must install
and work with zero runtime dependencies. The extra is declared as
`[project.optional-dependencies] pandas` and installed only in the first CI
job; `tests/test_pandas_api.py` skips wholesale via `HAS_PANDAS`, which is why
the cibuildwheel wheel tests stay pandas-free.

**Dimension limits are duplicated in three places and must stay in sync:**
`cc_dim_ok` (ccharts.h), `check_dimensions` (wrapper.c), `_check_dimensions`
(`__init__.py`). C returns an empty string for bad dimensions; Python raises
`ValueError` (and `TypeError` for non-int / bool dimensions). The point is that
no input path can trigger a giant or overflowing allocation.

**Timestamps.** `cc_parse_ts` accepts ISO8601 or plain epoch seconds; ISO
detection requires an exact `YYYY-MM-DD` prefix so negative epoch numbers aren't
misread. `cc_iso8601_to_epoch` applies the UTC offset (`+HH:MM`, `+HHMM`, `+HH`,
`Z`) using Hinnant's civil-days algorithm, so the stored epoch is the true
instant. Malformed input parses to `0`, which suppresses the footer.

## Constraints when editing

- **Portable C, no VLAs.** The header must build under MSVC: all scratch buffers
  are heap-allocated and freed on every path (including the allocation-failure
  path), `CC_INLINE` gives functions internal linkage, and `CC_GMTIME_R` wraps
  `gmtime_r`/`gmtime_s`. Don't introduce VLAs, C99-only constructs, or
  compiler-specific extensions.
- **Block characters are `\uXXXX` UCNs**, not literal UTF-8. `setup.py` passes
  `/utf-8` on MSVC so they survive; keep new glyphs in the same UCN form.
- Adding a C source file means updating both `setup.py` (extension sources) and
  `MANIFEST.in` (sdist contents) — `MANIFEST.in` does not affect wheels.

## Releasing

Bump `version` in `pyproject.toml`, then push a tag `v<version>`.
`.github/workflows/publish.yml` fails the release if the tag and the pyproject
version disagree, then builds the sdist plus cibuildwheel wheels (CPython
3.9–3.14; Linux x86_64, macOS x86_64/arm64, Windows AMD64) and uploads to PyPI
with the `PYPI_API_TOKEN` secret. Linux aarch64 is intentionally not built (QEMU
emulation on GitHub runners is too slow).
