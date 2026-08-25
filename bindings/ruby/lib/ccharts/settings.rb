# frozen_string_literal: true

module Ccharts
  # Settings builders for each chart type. Every field is optional — unset
  # fields take the C library's defaults (which is what the conformance suite
  # checks byte-for-byte), and `plain` forces no ANSI escapes at all.
  #
  # Each builder has a {#to_ffi} that materialises the matching C struct and
  # returns [struct, keepalive], where keepalive holds every backing
  # Fiddle::Pointer (labels, color escapes, ...) alive across the render call.
  module Settings
    # line / candle
    class ChartSettings
      def initialize
        @rise = @fall = @bg = @area = nil
        @single_color = false
        @show_prices = false
        @show_times = false
        @plain = false
      end

      def rise(v);  @rise = v;  self; end
      def fall(v);  @fall = v;  self; end
      def background(v); @bg = v; self; end
      def area(v);  @area = v;  self; end
      def single_color(v); @single_color = !!v; self; end
      def show_prices(v); @show_prices = !!v; self; end
      def show_times(v);  @show_times = !!v;  self; end
      def plain(v);  @plain = !!v;  self; end

      def to_ffi
        s = FFI::Settings.malloc
        keep = []
        set = lambda do |field, val|
          p = FFI.color_ptr(val, @plain)
          keep << p
          s[field] = p.to_i
        end
        set.call(:rise_color, @rise)
        set.call(:fall_color, @fall)
        set.call(:bg_color, @bg)
        set.call(:area_color, @area)
        s[:single_color] = @single_color ? 1 : 0
        s[:show_prices] = @show_prices ? 1 : 0
        s[:show_times] = @show_times ? 1 : 0
        [s, keep]
      end
    end

    # pie
    class PieSettings
      def initialize
        @donut = false
        @colors = nil
        @show_legend = true
        @show_pct = false
        @slice_gap = 0.0
        @inner_radius_ratio = -1.0
        @legend_format = 0
        @start_angle = -1.0
        @counter_clockwise = false
        @center_text = nil
      end

      def donut(v); @donut = !!v; self; end
      def colors(v); @colors = v; self; end
      def show_legend(v); @show_legend = !!v; self; end
      def show_pct(v); @show_pct = !!v; self; end
      def slice_gap(v); @slice_gap = v; self; end
      def inner_radius_ratio(v); @inner_radius_ratio = v; self; end
      def legend_format(v); @legend_format = v; self; end
      def start_angle(v); @start_angle = v; self; end
      def counter_clockwise(v); @counter_clockwise = !!v; self; end
      def center_text(v); @center_text = v; self; end

      # Packed scalar arguments in ccharts_pie_from_slices order (the settings
      # struct has no pie-specific fields; they are flat function arguments).
      def _scalars
        [@donut ? 1 : 0, @show_legend ? 1 : 0, @show_pct ? 1 : 0,
         @slice_gap, @inner_radius_ratio, @legend_format, @start_angle,
         @counter_clockwise ? 1 : 0]
      end

      def colors_count
        @colors ? @colors.length : 0
      end

      def center_text_ptr(keep)
        return Fiddle::NULL if @center_text.nil?
        p = FFI.cstr(@center_text)
        keep << p
        p
      end

      # Array of color escape pointers (index mod count), or Fiddle::NULL.
      def colors_ptr(keep)
        return Fiddle::NULL if @colors.nil? || @colors.empty?
        ptrs = @colors.map do |c|
          p = FFI.color_ptr(c, false)
          keep << p
          p
        end
        Fiddle::Pointer[ptrs.pack("Q*")]
      end
    end

    # histogram
    class HistSettings
      def initialize
        @rise = nil
        @bg = nil
        @bin_count = 0
        @min_value = Float::NAN
        @max_value = Float::NAN
        @show_bins = false
        @show_prices = false
        @plain = false
      end

      def rise(v); @rise = v; self; end
      def background(v); @bg = v; self; end
      def bin_count(v); @bin_count = v; self; end
      def min_value(v); @min_value = v; self; end
      def max_value(v); @max_value = v; self; end
      def show_bins(v); @show_bins = !!v; self; end
      def show_prices(v); @show_prices = !!v; self; end
      def plain(v); @plain = !!v; self; end

      def to_ffi
        s = FFI::HistSettings.malloc
        keep = []
        set = lambda do |field, val|
          p = FFI.color_ptr(val, @plain)
          keep << p
          s[field] = p.to_i
        end
        set.call(:rise_color, @rise)
        set.call(:bg_color, @bg)
        s[:bin_count] = @bin_count
        s[:min_value] = @min_value
        s[:max_value] = @max_value
        s[:show_bins] = @show_bins ? 1 : 0
        s[:show_prices] = @show_prices ? 1 : 0
        [s, keep]
      end
    end

    # sparkline
    class SparkSettings
      def initialize
        @rise = nil
        @area = nil
        @min_above = 0
        @min_below = 0
        @plain = false
      end

      def rise(v); @rise = v; self; end
      def area(v); @area = v; self; end
      def min_above(v); @min_above = v; self; end
      def min_below(v); @min_below = v; self; end
      def plain(v); @plain = !!v; self; end

      def to_ffi
        s = FFI::SparkSettings.malloc
        keep = []
        set = lambda do |field, val|
          p = FFI.color_ptr(val, @plain)
          keep << p
          s[field] = p.to_i
        end
        set.call(:rise_color, @rise)
        set.call(:area_color, @area)
        s[:min_above] = @min_above
        s[:min_below] = @min_below
        [s, keep]
      end
    end

    # bar
    class BarSettings
      def initialize
        @rise = nil
        @bg = nil
        @show_labels = false
        @show_prices = false
        @plain = false
      end

      def rise(v); @rise = v; self; end
      def background(v); @bg = v; self; end
      def show_labels(v); @show_labels = !!v; self; end
      def show_prices(v); @show_prices = !!v; self; end
      def plain(v); @plain = !!v; self; end

      def to_ffi
        s = FFI::BarSettings.malloc
        keep = []
        set = lambda do |field, val|
          p = FFI.color_ptr(val, @plain)
          keep << p
          s[field] = p.to_i
        end
        set.call(:rise_color, @rise)
        set.call(:bg_color, @bg)
        s[:show_labels] = @show_labels ? 1 : 0
        s[:show_prices] = @show_prices ? 1 : 0
        [s, keep]
      end
    end

    # stacked bar
    class StackSettings
      def initialize
        @colors = nil
        @bg = nil
        @cat_labels = nil
        @show_labels = false
        @show_prices = false
        @plain = false
      end

      def colors(v); @colors = v; self; end
      def background(v); @bg = v; self; end
      def category_labels(v); @cat_labels = v; self; end
      def show_labels(v); @show_labels = !!v; self; end
      def show_prices(v); @show_prices = !!v; self; end
      def plain(v); @plain = !!v; self; end

      # inject the series/category counts known only at render time
      def counts(series_count, cats_count)
        @series_count = series_count
        @cats_count = cats_count
        self
      end

      def to_ffi
        s = FFI::StackSettings.malloc
        keep = []

        # NULL-terminated per-series color pointer array.
        color_ptrs = []
        if @plain
          @series_count.times { color_ptrs << FFI::EMPTY.to_i }
        elsif @colors && !@colors.empty?
          @colors.each do |c|
            p = FFI.color_ptr(c, false)
            keep << p
            color_ptrs << p.to_i
          end
        end
        colors_addr = if color_ptrs.empty?
                        Fiddle::NULL
                      else
                        Fiddle::Pointer[(color_ptrs + [0]).pack("Q*")].to_i
                      end

        bgp = FFI.color_ptr(@bg, @plain)
        keep << bgp

        cat_ptrs = []
        if @cat_labels && !@cat_labels.empty?
          @cat_labels.each do |l|
            p = FFI.cstr(l)
            keep << p
            cat_ptrs << p.to_i
          end
        end
        cat_addr = cat_ptrs.empty? ? Fiddle::NULL : Fiddle::Pointer[cat_ptrs.pack("Q*")].to_i

        s.colors = colors_addr
        s.bg_color = bgp.to_i
        s.cat_labels = cat_addr
        s.series = @series_count
        s.cats = @cats_count
        s.show_labels = @show_labels ? 1 : 0
        s.show_prices = @show_prices ? 1 : 0
        [s, keep]
      end
    end

    # heatmap
    class HeatSettings
      def initialize
        @low = nil
        @high = nil
        @mid = nil
        @bg = nil
        @row_labels = nil
        @col_labels = nil
        @show_labels = false
        @plain = false
      end

      def low_color(v); @low = v; self; end
      def high_color(v); @high = v; self; end
      def mid_color(v); @mid = v; self; end
      def background(v); @bg = v; self; end
      def row_labels(v); @row_labels = v; self; end
      def col_labels(v); @col_labels = v; self; end
      def show_labels(v); @show_labels = !!v; self; end
      def plain(v); @plain = !!v; self; end

      def label_ptr_array(labels, keep)
        return Fiddle::NULL if labels.nil? || labels.empty?
        ptrs = labels.map do |l|
          p = FFI.cstr(l)
          keep << p
          p.to_i
        end
        Fiddle::Pointer[ptrs.pack("Q*")]
      end

      def to_ffi
        s = FFI::HeatSettings.malloc
        keep = []
        set = lambda do |field, val|
          p = FFI.color_ptr(val, @plain)
          keep << p
          s[field] = p.to_i
        end
        set.call(:low_color, @low)
        set.call(:high_color, @high)
        set.call(:mid_color, @mid)
        set.call(:bg_color, @bg)
        s[:row_labels] = label_ptr_array(@row_labels, keep).to_i
        s[:col_labels] = label_ptr_array(@col_labels, keep).to_i
        s[:show_labels] = @show_labels ? 1 : 0
        [s, keep]
      end
    end

    # box plot
    class BoxSettings
      def initialize
        @rise = nil
        @area = nil
        @bg = nil
        @show_prices = false
        @plain = false
      end

      def rise(v); @rise = v; self; end
      def area(v); @area = v; self; end
      def background(v); @bg = v; self; end
      def show_prices(v); @show_prices = !!v; self; end
      def plain(v); @plain = !!v; self; end

      def to_ffi
        s = FFI::BoxSettings.malloc
        keep = []
        set = lambda do |field, val|
          p = FFI.color_ptr(val, @plain)
          keep << p
          s[field] = p.to_i
        end
        set.call(:rise_color, @rise)
        set.call(:area_color, @area)
        set.call(:bg_color, @bg)
        s[:show_prices] = @show_prices ? 1 : 0
        [s, keep]
      end
    end
  end
end
