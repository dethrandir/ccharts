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

## Chart types (core + ABI + Python + conformance first, then all bindings)
- [x] **Histogram** — frequency distribution of continuous values.
- [x] **Sparkline** — ultra-compact trend line.
- [x] **Bar chart** — categorical values as vertical bars.
- [x] **Stacked bar chart** — multiple series stacked per category.
- [x] **Heatmap** — 2D grid of colored cells for a matrix of scalar values.
- [x] **Box plot** — five-number summary (min, Q1, median, Q3, max) per category.

## New binding(s)
- [x] **Lua binding + LuaRocks package.**
- [x] **Ruby binding + gem.**
- [x] **Julia binding + package.**

---

## Done
- [x] All 6 new chart types implemented across core, ABI, Python, and all bindings.
- [x] Lua, Ruby, and Julia bindings added with 100% test and conformance suite parity.
- [x] Release workflows configured for RubyGems and LuaRocks.
- [x] v3.0.0 release preparation.