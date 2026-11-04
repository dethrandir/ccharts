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

The initial local checks above preceded GitHub publication. Existing release
tags predate the root iyi manifest; see the follow-up evidence below for the
current Git distribution and CI state.

## GitHub follow-up — 2026-11-04

- Implementation commit `ce1386ab283f` and documentation/test follow-up
  `e685fce39402` were pushed to `main`, preserving upstream README changes.
- A clean external consumer performed real GitHub `iyi get
  github.com/dethrandir/ccharts/v3@main`, received the first commit as a pinned
  pseudo-version, wrote `iyi.sum`, and rendered through the native archive.
- The GitHub repository now has the `iyi` topic. Local tests and documentation
  link checks passed after the follow-up changes.
- [CI run 41879791662](https://github.com/dethrandir/ccharts/actions/runs/41879791662)
  targets `e685fce39402`, with 29 jobs including Linux, macOS 15 arm64 and
  Windows iyi tests. At recording time, all jobs were queued without assigned
  runners; no CI pass is claimed. The older superseded run was cancelled.
- GitHub reports an [active Actions incident](https://www.githubstatus.com/incidents/h02n9572mx9y)
  beginning at 17:40 UTC (20:40 Europe/Istanbul), matching the queue delay.
  This is an external CI availability limit, not evidence that tests passed
  or failed. macOS and Windows execution remain pending.
- No release tag or iyipacks catalog listing was published. The current
  catalog rejects canonical `/v3` identities; the researched publication path
  and upstream compatibility fix are described in [PUBLICATION.md](PUBLICATION.md).


## CI portability fixes and latest observed state

The first run exposed two real failures, now fixed and pushed:

- `bcd21b7`: keep `-lccharts -lm` in one ordered linker fragment. Ubuntu's
  `--as-needed` discarded math symbols when separate annotations reversed the
  libraries. The failure was reproduced locally with an as-needed compiler
  wrapper; the full iyi tests then passed under that setting.
- `72bd486`: add the missing trailing newline to the public C header and sync
  every vendored header. Strict Clang compilation and unchanged 70-case C
  goldens passed after the formatting-only change.

[Current CI run 41880848815](https://github.com/dethrandir/ccharts/actions/runs/41880848815)
executes code commit `72bd4868c5f9`. At the latest observation, **26 of 29 jobs
passed**, including the full Linux and Windows iyi jobs. The remaining three
macOS jobs (iyi, Rust, C ABI) were queued without runner assignments. The run
has no failure reported, but is not complete and is not claimed green.
Duplicate/superseded runs were cancelled; the old failed run is retained as
failure evidence. The current Git package was fetched again from GitHub and
built successfully in a separate consumer with `--as-needed` enabled.
