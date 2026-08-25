# frozen_string_literal: true

require "fiddle"
require "fiddle/import"
require "rbconfig"

module Ccharts
  # Raw Fiddle bindings to the vendored ccharts ABI (ext/ccharts/vendor).
  #
  # The extension (ccharts_ext.so) is built from ext/ccharts/vendor/ccharts_abi.c
  # by mkmf — it is pure object code, so Fiddle just dlopens it and resolves
  # the 20 ccharts_* symbols below. The struct layouts mirror abi/ccharts_abi.h
  # exactly (field order + C alignment); a mismatch shows up as a conformance
  # failure, not a crash.
  module FFI
    extend Fiddle::Importer

    DLEXT = RbConfig::CONFIG["DLEXT"] # e.g. "so"

    # ---- native library location ---------------------------------------
    def self.native_candidates
      lib_dir = File.expand_path("..", __dir__)           # .../lib
      here = __dir__                                      # .../lib/ccharts
      repo_ext = File.expand_path("../../ext/ccharts", here) # repo build dir
      [
        File.join(lib_dir, "ccharts_ext.#{DLEXT}"),       # dev build drops it in lib/
        File.join(here, "ccharts_ext.#{DLEXT}"),          # some install layouts
        File.join(repo_ext, "ccharts_ext.#{DLEXT}"),      # built in-place
      ]
    end

    LIB = begin
      path = native_candidates.find { |p| File.file?(p) }
      raise LoadError, "ccharts native (ccharts_ext.#{DLEXT}) not built — run `rake compile` (or the Rakefile :compile task) first" unless path
      Fiddle.dlopen(path)
    end

    # ---- type constants -------------------------------------------------
    T_VOID   = Fiddle::TYPE_VOID
    T_VOIDP  = Fiddle::TYPE_VOIDP
    T_INT    = Fiddle::TYPE_INT
    T_DOUBLE = Fiddle::TYPE_DOUBLE
    T_CHAR   = Fiddle::TYPE_CHAR
    T_LL     = Fiddle::TYPE_LONG_LONG

    def self.fn(name, args, ret)
      Fiddle::Function.new(LIB[name], args, ret)
    end

    # ---- the 20 exported ccharts_* functions ----------------------------
    FROM_ARRAYS  = fn("ccharts_from_arrays", [T_VOIDP, T_VOIDP, T_VOIDP, T_VOIDP, T_VOIDP, T_INT, T_VOIDP], T_INT)
    PARSE_JSON   = fn("ccharts_parse_json", [T_VOIDP, T_VOIDP], T_INT)
    PARSE_CSV    = fn("ccharts_parse_csv", [T_VOIDP, T_CHAR, T_CHAR, T_VOIDP], T_INT)
    DATA_LEN     = fn("ccharts_data_len", [T_VOIDP], T_INT)
    DATA_FREE    = fn("ccharts_data_free", [T_VOIDP], T_VOID)
    STRING_FREE  = fn("ccharts_string_free", [T_VOIDP], T_VOID)
    LINE         = fn("ccharts_line", [T_VOIDP, T_INT, T_INT, T_VOIDP, T_VOIDP, T_VOIDP], T_INT)
    CANDLE       = fn("ccharts_candle", [T_VOIDP, T_INT, T_INT, T_VOIDP, T_VOIDP, T_VOIDP], T_INT)
    PIE          = fn("ccharts_pie_from_slices",
                      [T_VOIDP, T_INT, T_INT, T_INT, T_INT, T_VOIDP, T_INT, T_INT, T_INT,
                       T_DOUBLE, T_DOUBLE, T_INT, T_DOUBLE, T_INT, T_VOIDP, T_VOIDP, T_VOIDP], T_INT)
    HIST         = fn("ccharts_hist", [T_VOIDP, T_INT, T_INT, T_INT, T_VOIDP, T_VOIDP, T_VOIDP], T_INT)
    SPARK        = fn("ccharts_spark", [T_VOIDP, T_INT, T_INT, T_INT, T_VOIDP, T_VOIDP, T_VOIDP], T_INT)
    BAR          = fn("ccharts_bar", [T_VOIDP, T_INT, T_INT, T_INT, T_VOIDP, T_VOIDP, T_VOIDP], T_INT)
    STACK        = fn("ccharts_stack", [T_VOIDP, T_INT, T_INT, T_INT, T_VOIDP, T_VOIDP, T_VOIDP], T_INT)
    HEAT         = fn("ccharts_heat", [T_VOIDP, T_INT, T_INT, T_INT, T_INT, T_VOIDP, T_VOIDP, T_VOIDP], T_INT)
    BOX          = fn("ccharts_box", [T_VOIDP, T_INT, T_INT, T_INT, T_VOIDP, T_VOIDP, T_VOIDP], T_INT)
    COLOR        = fn("ccharts_color", [T_INT], T_VOIDP)
    ERROR_MSG    = fn("ccharts_error_message", [T_INT], T_VOIDP)
    VERSION      = fn("ccharts_version", [], T_VOIDP)
    MAX_DIM      = fn("ccharts_max_dim", [], T_INT)
    MAX_CELLS    = fn("ccharts_max_cells", [], T_INT)

    # ---- settings structs (layouts from abi/ccharts_abi.h) --------------
    # Every color is a pointer (void*); ints are int32. The Fiddle importer
    # applies the same alignment/padding rules as the C compiler, so these
    # match the ABI byte-for-byte.
    Settings = struct [
      "void* rise_color",
      "void* fall_color",
      "void* bg_color",
      "void* area_color",
      "int single_color",
      "int show_prices",
      "int show_times",
    ]

    PieSlice = struct [
      "void* label",
      "double value",
    ]

    HistSettings = struct [
      "void* rise_color",
      "void* bg_color",
      "int bin_count",
      "double min_value",
      "double max_value",
      "int show_bins",
      "int show_prices",
    ]

    SparkSettings = struct [
      "void* rise_color",
      "void* area_color",
      "int min_above",
      "int min_below",
    ]

    BarSlice = struct [
      "void* label",
      "double value",
    ]

    BarSettings = struct [
      "void* rise_color",
      "void* bg_color",
      "int show_labels",
      "int show_prices",
    ]

    StackSeries = struct [
      "void* name",
      "void* values",
    ]

    StackSettings = struct [
      "void* colors",
      "void* bg_color",
      "void* cat_labels",
      "int series",
      "int cats",
      "int show_labels",
      "int show_prices",
    ]

    HeatSettings = struct [
      "void* low_color",
      "void* high_color",
      "void* mid_color",
      "void* bg_color",
      "void* row_labels",
      "void* col_labels",
      "int show_labels",
    ]

    BoxCategory = struct [
      "void* name",
      "void* samples",
      "int n",
    ]

    BoxSettings = struct [
      "void* rise_color",
      "void* area_color",
      "void* bg_color",
      "int show_prices",
    ]

    # ---- helpers ---------------------------------------------------------
    # A never-referenced empty string (plain mode: no ANSI escape at all).
    EMPTY = Fiddle::Pointer["\0"]

    # Turn a color value into a NUL-terminated escape pointer. `plain` forces
    # the empty escape (overriding even a caller color) so no ANSI bytes are
    # emitted. nil -> 0 (library default); else the resolved escape string.
    def self.color_ptr(value, plain)
      return EMPTY if plain
      esc = Color.resolve(value)
      return Fiddle::NULL if esc.nil?
      Fiddle::Pointer[esc.b + "\0"]
    end

    # Pack a Ruby array of doubles into a C double[] pointer.
    def self.pack_doubles(arr)
      Fiddle::Pointer[arr.pack("d*")]
    end

    # Build ONE contiguous C array of `count` structs, each `size` bytes, so a
    # `const ccharts_foo*`/array-of-structs argument points at a real C-layout
    # block (`&arr[i]` is at `base + i*size`). Do NOT pass an array of pointers
    # to separately-malloc'd structs — C reads this as a contiguous block and a
    # pointer-array makes it read garbage at offset `i*size` (segfault / wrong
    # data). `rows` is one entry per element: an array of `[offset, nbytes,
    # pack_fmt, value]` field setters relative to that element's base.
    def self.contiguous_structs(size, rows)
      block = Fiddle::Pointer.malloc(size * rows.length)
      rows.each_with_index do |fields, i|
        base = i * size
        fields.each { |off, nbytes, fmt, val| block[base + off, nbytes] = [val].pack(fmt) }
      end
      block
    end

    # Pack a Ruby array of integers into a C int64_t[] pointer.
    def self.pack_i64(arr)
      Fiddle::Pointer[arr.pack("q*")]
    end

    # NUL-terminated pointer for a Ruby string (or Fiddle::NULL for nil).
    def self.cstr(str)
      return Fiddle::NULL if str.nil?
      Fiddle::Pointer[str.b + "\0"]
    end

    # Read a C string (NUL-terminated) from a returned pointer.
    def self.read_cstr(ptr)
      return nil if ptr.nil? || ptr.to_i == 0
      ptr.to_s
    end

    # Read the integer value pointed to by a Fiddle::Pointer of the given size.
    def self.read_ptr_int(buf, size)
      buf[0, size].unpack1(size == 8 ? "Q" : "L")
    end
  end
end
