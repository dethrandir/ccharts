# frozen_string_literal: true

module Ccharts
  # High-level chart API. A dataset built with {Chart.from_arrays},
  # {Chart.from_json} or {Chart.from_csv} is immutable; the pie / histogram /
  # sparkline / bar / stacked_bar / heatmap / boxplot renderers take their data
  # directly (they have no OHLC dataset, exactly like the C ABI they wrap).
  class Chart
    # ------------------------------------------------------------------
    # Building a dataset
    # ------------------------------------------------------------------

    # Build a dataset from four equal-length price columns and optional epoch
    # seconds. `ts` may be nil (all timestamps unknown).
    def self.from_arrays(open:, high:, low:, close:, ts: nil)
      n = open.length
      raise Error, "need at least one candle" if n == 0
      unless [high, low, close].all? { |a| a.length == n }
        raise Error, "open, high, low and close must have the same length"
      end
      if ts && ts.length != n
        raise Error, "ts must have the same length as the price columns"
      end

      open_ptr = FFI.pack_doubles(open)
      high_ptr = FFI.pack_doubles(high)
      low_ptr = FFI.pack_doubles(low)
      close_ptr = FFI.pack_doubles(close)
      ts_ptr = ts ? FFI.pack_i64(ts) : Fiddle::NULL

      out = Fiddle::Pointer.malloc(Fiddle::SIZEOF_VOIDP)
      status = FFI::FROM_ARRAYS.call(open_ptr, high_ptr, low_ptr, close_ptr, ts_ptr, n, out)
      _from_data_status(status, out)
    end

    # Build a dataset from the fixed-schema JSON document accepted by the
    # C layer (an array of {ts, open, high, low, close} objects).
    def self.from_json(text)
      json = FFI.cstr(text)
      out = Fiddle::Pointer.malloc(Fiddle::SIZEOF_VOIDP)
      status = FFI::PARSE_JSON.call(json, out)
      _from_data_status(status, out)
    end

    # Build a dataset from CSV rows of `open,high,low,close[,timestamp]`.
    def self.from_csv(text, value_separator: ",", line_separator: "\n")
      csv = FFI.cstr(text)
      vs = value_separator.ord
      ls = line_separator.ord
      raise Error, "separators must not be NUL" if vs == 0 || ls == 0
      out = Fiddle::Pointer.malloc(Fiddle::SIZEOF_VOIDP)
      status = FFI::PARSE_CSV.call(csv, vs, ls, out)
      _from_data_status(status, out)
    end

    def self._from_data_status(status, out)
      Ccharts.raise_on(status)
      addr = FFI.read_ptr_int(out, Fiddle::SIZEOF_VOIDP)
      raise Error, "no dataset handle returned" if addr.zero?
      _from_handle(Fiddle::Pointer.new(addr))
    end

    # ------------------------------------------------------------------
    # Rendering an OHLC dataset (line / candle)
    # ------------------------------------------------------------------

    def line(width, height, settings = nil)
      _render(FFI::LINE, width, height, settings || Settings::ChartSettings.new)
    end

    def candle(width, height, settings = nil)
      _render(FFI::CANDLE, width, height, settings || Settings::ChartSettings.new)
    end

    # Number of candles in this dataset.
    def size
      FFI::DATA_LEN.call(@handle)
    end
    alias_method :length, :size

    # ------------------------------------------------------------------
    # Pie / histogram / sparkline / bar / stack / heat / box renderers
    # ------------------------------------------------------------------

    def self.pie(slices, width, height, settings = nil)
      raise Error, "need at least one slice" if slices.empty?
      settings ||= Settings::PieSettings.new

      keep = []
      rows = slices.map do |sl|
        label = sl[:label] || sl["label"]
        value = (sl[:value] || sl["value"] || 0.0).to_f
        lp = FFI.cstr(label)
        keep << lp
        [[0, 8, "Q", lp.to_i], [8, 8, "d", value]] # label @0, value @8
      end
      slices_ptr = FFI.contiguous_structs(16, rows) # ccharts_pie_slice = 16 B
      keep << slices_ptr

      colors_ptr_addr = settings.colors_ptr(keep).to_i
      center_ptr = settings.center_text_ptr(keep)
      donut, show_legend, show_pct, slice_gap, inner_radius_ratio,
        legend_format, start_angle, counter_clockwise = settings._scalars

      # `_scalars` already returns the flags as C ints 0/1 — pass them straight
      # through. Do NOT write `x ? 1 : 0` here: in Ruby `0` is truthy, so a
      # zeroed flag would be coerced to 1 (pct/ccw always on → mirrored pie).
      _read_string(
        FFI::PIE, slices_ptr, slices.length, width, height,
        donut, colors_ptr_addr, settings.colors_count,
        show_legend, show_pct,
        slice_gap, inner_radius_ratio, legend_format,
        start_angle, counter_clockwise, center_ptr
      )
    end

    def self.histogram(samples, width, height, settings = nil)
      raise Error, "need at least one sample" if samples.empty?
      settings ||= Settings::HistSettings.new
      ptr = FFI.pack_doubles(samples)
      s, keep = settings.to_ffi
      _read_string(FFI::HIST, ptr, samples.length, width, height, s.to_ptr)
    end

    def self.sparkline(samples, width, height, settings = nil)
      raise Error, "need at least one sample" if samples.empty?
      settings ||= Settings::SparkSettings.new
      ptr = FFI.pack_doubles(samples)
      s, keep = settings.to_ffi
      _read_string(FFI::SPARK, ptr, samples.length, width, height, s.to_ptr)
    end

    def self.bar(labels, values, width, height, settings = nil)
      raise Error, "need at least one bar" if labels.empty?
      raise Error, "labels and values must have the same length" unless labels.length == values.length
      settings ||= Settings::BarSettings.new

      keep = []
      rows = labels.zip(values).map do |lbl, val|
        lp = FFI.cstr(lbl)
        keep << lp
        [[0, 8, "Q", lp.to_i], [8, 8, "d", val.to_f]] # label @0, value @8
      end
      items_ptr = FFI.contiguous_structs(16, rows) # ccharts_bar_slice = 16 B
      keep << items_ptr
      s, skeep = settings.to_ffi
      keep.concat(skeep)
      _read_string(FFI::BAR, items_ptr, labels.length, width, height, s.to_ptr)
    end

    # series: array of {name:, values:} hashes sharing one category count.
    def self.stacked_bar(series, width, height, settings = nil)
      raise Error, "need at least one series" if series.empty?
      cats = series[0][:values]&.length || series[0]["values"].length
      raise Error, "series values must not be empty" if cats == 0
      series.each do |sv|
        vs = sv[:values] || sv["values"]
        raise Error, "all series must have the same number of values" unless vs.length == cats
      end
      settings ||= Settings::StackSettings.new
      settings.counts(series.length, cats)

      keep = []
      rows = series.map do |sv|
        vs = sv[:values] || sv["values"]
        name = sv[:name] || sv["name"]
        np = FFI.cstr(name)
        vp = FFI.pack_doubles(vs)
        keep << np << vp
        [[0, 8, "Q", np.to_i], [8, 8, "Q", vp.to_i]] # name @0, values @8
      end
      series_ptr = FFI.contiguous_structs(16, rows) # ccharts_stack_series = 16 B
      keep << series_ptr
      s, skeep = settings.to_ffi
      keep.concat(skeep)
      _read_string(FFI::STACK, series_ptr, series.length, width, height, s.to_ptr)
    end

    # values: a rows x cols matrix (array of equal-length arrays).
    def self.heatmap(values, width, height, settings = nil)
      raise Error, "need a non-empty value matrix" if values.empty?
      cols = values[0].length
      raise Error, "matrix columns must not be empty" if cols == 0
      values.each { |row| raise Error, "all rows must have the same number of values" unless row.length == cols }
      settings ||= Settings::HeatSettings.new

      flat = values.flatten
      flat_ptr = FFI.pack_doubles(flat)
      s, keep = settings.to_ffi
      _read_string(FFI::HEAT, flat_ptr, values.length, cols, width, height, s.to_ptr)
    end

    # series: array of {name:, samples:} hashes (per-category samples).
    def self.boxplot(series, width, height, settings = nil)
      raise Error, "need at least one category" if series.empty?
      series.each do |c|
        sm = c[:samples] || c["samples"]
        raise Error, "every category must have at least one sample" if sm.empty?
      end
      settings ||= Settings::BoxSettings.new

      keep = []
      rows = series.map do |c|
        sm = c[:samples] || c["samples"]
        name = c[:name] || c["name"]
        np = FFI.cstr(name)
        sp = FFI.pack_doubles(sm)
        keep << np << sp
        [[0, 8, "Q", np.to_i], [8, 8, "Q", sp.to_i], [16, 4, "l", sm.length]] # name@0, samples@8, n@16 (24 B)
      end
      cats_ptr = FFI.contiguous_structs(24, rows) # ccharts_box_category = 24 B
      keep << cats_ptr
      s, skeep = settings.to_ffi
      keep.concat(skeep)
      _read_string(FFI::BOX, cats_ptr, series.length, width, height, s.to_ptr)
    end

    # ------------------------------------------------------------------
    # Internals
    # ------------------------------------------------------------------

    def self._from_handle(handle)
      obj = allocate
      obj.instance_variable_set(:@handle, handle)
      ObjectSpace.define_finalizer(obj, _finalizer(handle.to_i))
      obj
    end

    def self._finalizer(addr)
      proc { FFI::DATA_FREE.call(Fiddle::Pointer.new(addr)) }
    end

    def _render(fn, width, height, settings)
      s, = settings.to_ffi
      self.class._read_string(fn, @handle, width, height, s.to_ptr)
    end

    # Call a render function and copy the returned C string, releasing it.
    def self._read_string(fn, *args)
      out = Fiddle::Pointer.malloc(Fiddle::SIZEOF_VOIDP)
      out_len = Fiddle::Pointer.malloc(Fiddle::SIZEOF_SIZE_T)
      status = fn.call(*args, out, out_len)
      Ccharts.raise_on(status)
      addr = FFI.read_ptr_int(out, Fiddle::SIZEOF_VOIDP)
      len = FFI.read_ptr_int(out_len, Fiddle::SIZEOF_SIZE_T)
      s = Fiddle::Pointer.new(addr).to_s(len).force_encoding("UTF-8")
      FFI::STRING_FREE.call(Fiddle::Pointer.new(addr))
      s
    end
    private_class_method :_from_handle, :_from_data_status, :_finalizer
    private :_render
  end
end
