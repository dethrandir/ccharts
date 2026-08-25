# ccharts — TODO

Planned feature / binding work, in roughly priority order. Item landing criteria:
the shared conformance suite (conformance/golden/*.txt) passes byte-for-byte in that
language, `scripts/check_versions.py` stays green, docs updated. See CLAUDE.md for the
architecture and RELEASING.md for release mechanics.

> **Ordering principle:** new chart TYPES come before new language BINDINGS. Each new
> chart first lands in the core + ABI + Python + conformance, then in every binding —
> so bundling a chart tipi + a new language into one change would make us touch that
> language twice. Add charts first; add languages last.

---

## Chart types (core + ABI + Python + conformance first, then all six bindings)
Each follows the pie playbook: `cc_<type>_create` in `ccharts.h`, an ABI symbol,
a Python `Chart.<type>`, conformance cases + goldens, then every binding + version bump.

- [ ] **Histogram** — frequency distribution of continuous values (bins over a range,
      bar heights = counts). Input: a sample sequence (+ optional bin count/range);
      axis labels optional.
- [ ] **Sparkline** — ultra-compact trend line, typically one row, no axis, tiny.
      Reuses the line chart's per-cell sub-pixel resolution (`cc_lower_eighth`).
- [ ] **Bar chart** — categorical values as vertical bars. Input: `(label, value)`
      pairs like pie; optional value axis. (Vertical resolution per the line chart.)
- [ ] **Stacked bar chart** — multiple series stacked per category, showing parts of a
      whole per bar. Input: series × categories matrix.
- [ ] **Heatmap** — 2D grid of colored cells for a matrix of scalar values; color
      scales with value (deterministic colormap, never random). Optional row/col labels.
- [ ] **Box plot** — five-number summary (min, Q1, median, Q3, max) per category,
      drawn with block/box-drawing characters. Input: per-category samples (stats
      computed in the renderer) or precomputed quartiles.

## New binding(s) — after charts are done
- [ ] **Lua binding + LuaRocks package.** Round out the single-header story with Lua.
      Vendored ABI (like Rust/Go/JS) or link shared lib (like C#/Java) — decide from
      what Lua packaging supports. Must run the shared conformance suite.
- [ ] **Ruby binding + gem.** Add `ccharts`/`ccharts.rb` exposing the same API
      (`from_arrays/from_json/from_csv` then `line/candle/pie/<charts>`). Gem packages
      the native lib per platform. Must pass the shared conformance suite.
- [ ] **Julia binding + package / Artifacts.** Add `Ccharts.jl` with the same API.
      Julia's Artifacts/`ccall` path for native loading. Must pass the shared conformance suite.

## Per new binding, remember (from how Go/Rust/JS/C#/Java were added)
- [ ] Vendored ABI copies refreshed via `scripts/sync_sources.py` (Rust/Go/JS-style) or
      shared-lib linking with `CCHARTS_NATIVE_DIR` fallback (C#/Java-style).
- [ ] Conformance test mirrors the shared suite with a truncation guard and asserts
      byte-for-byte equality against `conformance/golden/*.txt`.
- [ ] A published package — needs the `natives.yml` + `pack_natives.py` write path if it
      ships binaries, and a release workflow (like `publish-npm.yml` / `publish-nuget.yml`).
- [ ] `scripts/check_versions.py` extended to the new manifest.
- [ ] Version single-tag bump across all manifests when the binding's first release ships.

---

## Done
- [x] Pie/donut chart (core + ABI + all six bindings + conformance) — b8b8200
- [x] Phase 3 pie settings (`slice_gap`, `inner_radius_ratio`, `legend_format`,
      `start_angle`+`counter_clockwise`, `center_text`) — c074ca6
- [x] Pie aspect-ratio fix (round disks on real terminals) — bc467c2
- [x] v0.2.2 release: pie settings + aspect fix + bug fixes to PyPI/crates/npm/NuGet/Maven + Go tag — 5fded69
- [x] Bug fix: reject non-finite JSON input; plug PyCapsule OOM leak — 4466fce