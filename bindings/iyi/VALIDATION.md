# iyi binding validation — 2026-11-04

Base: ccharts commit `caf6c424e68c8b75828d459dcead7338af0ccb82` (3.0.0).
Environment: Linux x86-64, iyi 0.20.1 (Crystal 1.22.0-dev base), GCC 15.3.1,
Python 3.14, CMake, Valgrind 3.27.1. All iyi checks used the normal iyi standard
library, without `--crystal`.

## Passed locally

- Native archive compiled with `-Wall -Wextra -Werror -pedantic`.
- `python3 bindings/iyi/scripts/dev.py test`: formatting, 9 named API tests,
  70 shared byte-for-byte conformance cases across all nine chart families,
  9 actual-binary CLI tests, and isolated external package-consumer acceptance.
- The external consumer performs real `iyi get` against a temporary git mirror
  containing the current source under a synthetic v3.0.0 tag. It uses the public
  `/v3` import, records `iyi.sum`, then removes the mirror and forces a fresh
  executable build using the cached package. It is not a test of an existing
  upstream release tag.
- Root CMake build and CTest smoke; `scripts/gen_golden.py --check`: 70 cases
  unchanged. Source-vendor drift and shared-version guards pass.
- Public quickstart with `defer chart.close` compiled and rendered.
- Real `fl market history THYAO 1m 1d --json` input: 21 OHLC records,
  2026-10-05 through 2026-11-03. Candle, line and spark rendering succeeded.
- Real `fl price ASELS --json` input: previous close 342.50 to price 354.75,
  source timestamp 2026-11-03. Quote rendering uses two observations and is not
  represented as intraday history. Both real inputs were parsed by iyi's
  `std/json`, timestamps by `std/time`, then passed through `Chart.from_arrays`.
  Network access was needed only to retrieve these external demo inputs.

## Native memory check and its limit

Valgrind with the default iyi GC crashed in `__iyi_enter` during runtime startup,
before any C allocations. The same failure reproduced in a no-import hello
program, so this check cannot qualify the normal runtime. It is an unresolved
compatibility limitation, not a passing leak check.

A supplementary, straight-line exercise built with `-Dgc_none` performed 500
native dataset/render/free cycles (candles and pie): **6,000 allocations,
6,000 frees, zero bytes left on the C heap and zero Valgrind errors**.
This checks C allocation ownership under that configuration, not GC correctness
or concurrent use. Reproduce from the repository root after the native build:

```sh
export IYI_LIBRARY_PATH="$PWD/bindings/iyi/build"
iyi build -Dgc_none bindings/iyi/test/native_memory.iyi -o bindings/iyi/build/memory-check
valgrind --leak-check=full --show-leak-kinds=definite \
  --errors-for-leak-kinds=definite --error-exitcode=9 bindings/iyi/build/memory-check
```

## Pending / scope

macOS arm64 and Windows x86-64 CI jobs are configured but have not been executed
in this delivery. No claim is made for 32-bit, WebAssembly or cross-compilation.
No existing language binding behavior or C rendering code was changed; the full
unrelated multi-language CI matrix was not run locally. Dataset handles require
explicit close and concurrent close/render needs caller coordination.

No GitHub push, release tag, topic change or catalog publication was performed.
Existing upstream tags predate the root iyi manifest. Publication needs a new
release containing the binding (or `@main` after merge), the public repository
metadata, and observation of the subsequent iyipacks import.
