# ccharts — TODO

Planned feature / binding work, in roughly priority order. Item landing criteria:
the shared conformance suite (conformance/golden/*.txt) passes byte-for-byte in that
language, `scripts/check_versions.py` stays green, docs updated. See CLAUDE.md for the
architecture and RELEASING.md for release mechanics.

---

## Feel free chart polish
- [ ] Pie aspect-ratio fix: make the renderer account for ~2:1 terminal cells so the
      disk looks round/circular instead of vertically stretched (current default draws
      a perfect math circle in square-cell space, which reads as an ellipse on real
      terminals). Add a single aspect factor in `cc_pie_create` radius geometry, then
      regenerate the pie goldens and re-verify every binding's conformance.

## New binding(s)
- [ ] **Lua binding + LuaRocks package.** Round out the single-header story with Lua.
      Vendored ABI (like Rust/Go/JS) or link shared lib (like C#/Java) — decide from
      what Lua packaging supports. Must run the shared conformance suite (35 cases).
- [ ] **Ruby binding + gem.** Add `ccharts`/`ccharts.rb` exposing the same
      `from_arrays/from_json/from_csv` then `line/candle/pie` API. Gem packages the
      native lib per platform. Must pass the shared conformance suite.
- [ ] **Julia binding + package / Artifacts.** Add `Ccharts.jl` with the same API.
      Julia's Artifacts/`ccall` path for native loading. Must pass the shared conformance
      suite.

## Per new binding, remember (from how Go/Rust/JS/C#/Java were added)
- [ ] Vendored ABI copies refreshed via `scripts/sync_sources.py` (Rust/Go/JS-style) or
      shared-lib linking with `CCHARTS_NATIVE_DIR` fallback (C#/Java-style).
- [ ] Conformance test mirrors the shared suite with the `>= 10` truncation guard and
      asserts byte-for-byte equality against `conformance/golden/*.txt`.
- [ ] A published package (LuaRocks / Rubygems / Julia registry) — needs the 
      `natives.yml` + `pack_natives.py` write path if it ships binaries, and a release
      workflow (like `publish-npm.yml` / `publish-nuget.yml`).
- [ ] `scripts/check_versions.py` extended to the new manifest.
- [ ] Version single-tag bump across all manifests when the binding's first release ships.

---

## Done
- [x] Pie/donut chart (core + ABI + all six bindings + conformance) — b8b8200
- [x] Phase 3 pie settings (`slice_gap`, `inner_radius_ratio`, `legend_format`,
      `start_angle`+`counter_clockwise`, `center_text`) — implemented in core/ABI/Python/
      conformance; bindings adaptation pending its own commit.
- [x] Bug fix: reject non-finite JSON input; plug PyCapsule OOM leak — 4466fce