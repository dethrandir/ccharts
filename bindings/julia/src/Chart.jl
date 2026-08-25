### src/Chart.jl — dataset construction + renderers, mirroring the C ABI.
#
# Dataset methods (from_arrays / from_json / from_csv) build a vendored
# ccharts_data handle that is freed by a finalizer (ccharts_data_free).
# Chart#line / Chart#candle render an OHLC dataset; the standalone renderers
# (pie / histogram / sparkline / bar / stacked_bar / heatmap / boxplot) take
# their own data. Every render copies the returned buffer into a String, then
# releases it with ccharts_string_free.
module __CchartsChart__

import ..Ccharts: _LIB, _raise_on, _cstr, _color, _ptr_array, _ESCAPES, NUL,
    CSettings, KPieSlice, CHistSettings, CSparkSettings, KBarSlice,
    CBarSettings, KStackSeries, CStackSettings, CHeatSettings, KBoxCategory,
    CBoxSettings, CchartsError

export Chart

import Base: size, length, getproperty

# Root the returned data handle; freed with ccharts_data_free on GC.
mutable struct DataHandle
    ptr::Ptr{Cvoid}
    function DataHandle(p::Ptr{Cvoid})
        d = new(p)
        finalizer(_finalize!, d)
        return d
    end
end
function _finalize!(d::DataHandle)
    p = d.ptr
    if p != C_NULL
        d.ptr = C_NULL
        ccall((:ccharts_data_free, _LIB), Cvoid, (Ptr{Cvoid},), p)
    end
end

"""An immutable OHLC dataset (opaque ccharts_data handle)."""
struct Chart
    handle::DataHandle
end
Base.size(c::Chart) = ccall((:ccharts_data_len, _LIB), Cint, (Ptr{Cvoid},), c.handle.ptr)
Base.length(c::Chart) = Base.size(c)

# ---------------------------------------------------------------------------
# Building a dataset
# ---------------------------------------------------------------------------

"""Build a dataset from four equal-length price columns; `ts` (epoch seconds)
may be `nothing`."""
function from_arrays(open, high, low, close; ts=nothing)
    n = length(open)
    n == 0 && throw(CchartsError(1, "need at least one candle"))
    if !(length(high) == n && length(low) == n && length(close) == n)
        throw(CchartsError(1, "open, high, low and close must have the same length"))
    end
    if ts !== nothing && length(ts) != n
        throw(CchartsError(1, "ts must have the same length as the price columns"))
    end
    open_v  = Float64.(open)
    high_v  = Float64.(high)
    low_v   = Float64.(low)
    close_v = Float64.(close)
    ts_v    = ts === nothing ? nothing : Int64.(ts)
    openp  = pointer(open_v)
    highp  = pointer(high_v)
    lowp   = pointer(low_v)
    closep = pointer(close_v)
    tsp    = ts_v === nothing ? Ptr{Int64}(C_NULL) : pointer(ts_v)
    out = Ref{Ptr{Cvoid}}(C_NULL)
    st = ccall((:ccharts_from_arrays, _LIB), Cint,
               (Ptr{Float64}, Ptr{Float64}, Ptr{Float64}, Ptr{Float64},
                Ptr{Int64}, Cint, Ref{Ptr{Cvoid}}),
               openp, highp, lowp, closep, tsp, Cint(n), out)
    _raise_on(st)
    p = out[]
    p == C_NULL && throw(CchartsError(1, "no dataset handle returned"))
    return Chart(DataHandle(p))
end

"""Build a dataset from the fixed-schema JSON accepted by the C layer."""
function from_json(text::AbstractString)
    out = Ref{Ptr{Cvoid}}(C_NULL)
    st = ccall((:ccharts_parse_json, _LIB), Cint,
               (Cstring, Ref{Ptr{Cvoid}}), String(text), out)
    _raise_on(st)
    p = out[]
    p == C_NULL && throw(CchartsError(1, "no dataset handle returned"))
    return Chart(DataHandle(p))
end

"""Build a dataset from CSV rows of `open,high,low,close[,timestamp]`."""
function from_csv(text::AbstractString; value_separator=',', line_separator='\n')
    vs = Cchar(Int(value_separator) & 0xff)
    ls = Cchar(Int(line_separator) & 0xff)
    (vs == 0 || ls == 0) && throw(CchartsError(1, "separators must not be NUL"))
    out = Ref{Ptr{Cvoid}}(C_NULL)
    st = ccall((:ccharts_parse_csv, _LIB), Cint,
               (Cstring, Cchar, Cchar, Ref{Ptr{Cvoid}}), String(text), vs, ls, out)
    _raise_on(st)
    p = out[]
    p == C_NULL && throw(CchartsError(1, "no dataset handle returned"))
    return Chart(DataHandle(p))
end

# Allow Chart.from_arrays, Chart.from_json, Chart.from_csv syntax
function Base.getproperty(::Type{Chart}, sym::Symbol)
    sym === :from_arrays && return from_arrays
    sym === :from_json   && return from_json
    sym === :from_csv    && return from_csv
    getfield(Chart, sym)
end

# ---------------------------------------------------------------------------
# Render driver
# ---------------------------------------------------------------------------

# Copy `out` (NUL-terminated, len bytes) then free it via ccharts_string_free.
function _copy_and_free(out::Ptr{UInt8}, outlen::Csize_t)
    out == C_NULL && return ""
    n = Int(outlen)
    buf = Vector{UInt8}(undef, n)
    unsafe_copyto!(pointer(buf), out, n)
    ccall((:ccharts_string_free, _LIB), Cvoid, (Ptr{UInt8},), out)
    return String(buf)
end

# ---------------------------------------------------------------------------
function line(c::Chart, w, h;
              rise_color=nothing, fall_color=nothing, bg_color=nothing,
              area_color=nothing, single_color::Bool=false,
              show_prices::Bool=false, show_times::Bool=false,
              plain::Bool=false)
    keep = Any[]
    s = CSettings(
        _color(rise_color, keep; plain), _color(fall_color, keep; plain),
        _color(bg_color, keep; plain), _color(area_color, keep; plain),
        Int32(single_color ? 1 : 0), Int32(show_prices ? 1 : 0),
        Int32(show_times ? 1 : 0))
    out = Ref{Ptr{UInt8}}(C_NULL); outlen = Ref{Csize_t}(0)
    st = ccall((:ccharts_line, _LIB), Cint,
               (Ptr{Cvoid}, Cint, Cint, Ref{CSettings},
                Ref{Ptr{UInt8}}, Ref{Csize_t}),
               c.handle.ptr, Cint(w), Cint(h), Ref(s), out, outlen)
    _raise_on(st)
    return _copy_and_free(out[], outlen[])
end

function candle(c::Chart, w, h;
                rise_color=nothing, fall_color=nothing, bg_color=nothing,
                area_color=nothing, single_color::Bool=false,
                show_prices::Bool=false, show_times::Bool=false,
                plain::Bool=false)
    keep = Any[]
    s = CSettings(
        _color(rise_color, keep; plain), _color(fall_color, keep; plain),
        _color(bg_color, keep; plain), _color(area_color, keep; plain),
        Int32(single_color ? 1 : 0), Int32(show_prices ? 1 : 0),
        Int32(show_times ? 1 : 0))
    out = Ref{Ptr{UInt8}}(C_NULL); outlen = Ref{Csize_t}(0)
    st = ccall((:ccharts_candle, _LIB), Cint,
               (Ptr{Cvoid}, Cint, Cint, Ref{CSettings},
                Ref{Ptr{UInt8}}, Ref{Csize_t}),
               c.handle.ptr, Cint(w), Cint(h), Ref(s), out, outlen)
    _raise_on(st)
    return _copy_and_free(out[], outlen[])
end

# ---------------------------------------------------------------------------
# pie
# ---------------------------------------------------------------------------

_normalize_slice(sl) = begin
    if sl isa Pair
        (string(first(sl)), Float64(last(sl)))
    elseif sl isa Tuple && length(sl) >= 2
        (string(sl[1]), Float64(sl[2]))
    elseif sl isa AbstractDict
        lbl = haskey(sl, :label) ? sl[:label] :
              haskey(sl, "label") ? sl["label"] : ""
        val = haskey(sl, :value) ? sl[:value] :
              haskey(sl, "value") ? sl["value"] : 0.0
        (string(lbl), Float64(val))
    elseif hasproperty(sl, :label) && hasproperty(sl, :value)
        (string(getproperty(sl, :label)), Float64(getproperty(sl, :value)))
    else
        throw(CchartsError(1, "invalid slice format"))
    end
end

function pie(slices, w, h;
             donut::Bool=false, colors=nothing, show_legend::Bool=true,
             show_pct::Bool=false, slice_gap::Real=0.0,
             inner_radius_ratio::Real=-1.0, legend_format::Integer=0,
             start_angle::Real=-1.0, counter_clockwise::Bool=false,
             center_text=nothing)
    slices = collect(slices)
    isempty(slices) && throw(CchartsError(1, "need at least one slice"))

    keep = Any[]
    arr = KPieSlice[]
    sizehint!(arr, length(slices))
    for sl in slices
        (lbl, val) = _normalize_slice(sl)
        push!(arr, KPieSlice(_cstr(lbl, keep), val))
    end
    push!(keep, arr)

    (colors_ptr, _) = _ptr_array(colors, keep)
    color_count = colors === nothing ? Cint(0) : Cint(length(colors))
    center = center_text === nothing ? NUL : _cstr(string(center_text), keep)

    out = Ref{Ptr{UInt8}}(C_NULL); outlen = Ref{Csize_t}(0)
    st = ccall((:ccharts_pie_from_slices, _LIB), Cint,
               (Ptr{KPieSlice}, Cint, Cint, Cint, Cint, Ptr{UInt8}, Cint,
                Cint, Cint, Float64, Float64, Cint, Float64, Cint, Ptr{UInt8},
                Ref{Ptr{UInt8}}, Ref{Csize_t}),
               pointer(arr), Cint(length(arr)), Cint(w), Cint(h),
               Cint(donut ? 1 : 0), colors_ptr, color_count,
               Cint(show_legend ? 1 : 0), Cint(show_pct ? 1 : 0),
               Float64(slice_gap), Float64(inner_radius_ratio),
               Cint(legend_format), Float64(start_angle),
               Cint(counter_clockwise ? 1 : 0), center, out, outlen)
    _raise_on(st)
    return _copy_and_free(out[], outlen[])
end

# ---------------------------------------------------------------------------
# histogram
# ---------------------------------------------------------------------------

function histogram(samples, w, h;
                   rise_color=nothing, bg_color=nothing, bin_count::Integer=0,
                   min_value::Real=NaN, max_value::Real=NaN,
                   show_bins::Bool=false, show_prices::Bool=false,
                   plain::Bool=false)
    samples = Float64.(collect(samples))
    isempty(samples) && throw(CchartsError(1, "need at least one sample"))
    keep = Any[]
    sp = pointer(samples); push!(keep, samples)
    s = CHistSettings(_color(rise_color, keep; plain), _color(bg_color, keep; plain),
                      Cint(bin_count), Float64(min_value), Float64(max_value),
                      Cint(show_bins ? 1 : 0), Cint(show_prices ? 1 : 0))
    out = Ref{Ptr{UInt8}}(C_NULL); outlen = Ref{Csize_t}(0)
    st = ccall((:ccharts_hist, _LIB), Cint,
               (Ptr{Float64}, Cint, Cint, Cint, Ref{CHistSettings},
                Ref{Ptr{UInt8}}, Ref{Csize_t}),
               sp, Cint(length(samples)), Cint(w), Cint(h), Ref(s), out, outlen)
    _raise_on(st)
    return _copy_and_free(out[], outlen[])
end

# ---------------------------------------------------------------------------
# sparkline
# ---------------------------------------------------------------------------

function sparkline(samples, w, h;
                   rise_color=nothing, area_color=nothing,
                   min_above::Integer=0, min_below::Integer=0,
                   plain::Bool=false)
    samples = Float64.(collect(samples))
    isempty(samples) && throw(CchartsError(1, "need at least one sample"))
    keep = Any[]
    sp = pointer(samples); push!(keep, samples)
    s = CSparkSettings(_color(rise_color, keep; plain), _color(area_color, keep; plain),
                       Cint(min_above), Cint(min_below))
    out = Ref{Ptr{UInt8}}(C_NULL); outlen = Ref{Csize_t}(0)
    st = ccall((:ccharts_spark, _LIB), Cint,
               (Ptr{Float64}, Cint, Cint, Cint, Ref{CSparkSettings},
                Ref{Ptr{UInt8}}, Ref{Csize_t}),
               sp, Cint(length(samples)), Cint(w), Cint(h), Ref(s), out, outlen)
    _raise_on(st)
    return _copy_and_free(out[], outlen[])
end

# ---------------------------------------------------------------------------
# bar
# ---------------------------------------------------------------------------

function bar(items, w, h;
             rise_color=nothing, bg_color=nothing,
             show_labels::Bool=false, show_prices::Bool=false,
             plain::Bool=false)
    items = collect(items)
    isempty(items) && throw(CchartsError(1, "need at least one bar"))
    keep = Any[]
    arr = KBarSlice[]
    sizehint!(arr, length(items))
    for it in items
        (lbl, val) = _normalize_slice(it)
        push!(arr, KBarSlice(_cstr(lbl, keep), val))
    end
    push!(keep, arr)
    s = CBarSettings(_color(rise_color, keep; plain), _color(bg_color, keep; plain),
                     Cint(show_labels ? 1 : 0), Cint(show_prices ? 1 : 0))
    out = Ref{Ptr{UInt8}}(C_NULL); outlen = Ref{Csize_t}(0)
    st = ccall((:ccharts_bar, _LIB), Cint,
               (Ptr{KBarSlice}, Cint, Cint, Cint, Ref{CBarSettings},
                Ref{Ptr{UInt8}}, Ref{Csize_t}),
               pointer(arr), Cint(length(arr)), Cint(w), Cint(h), Ref(s), out, outlen)
    _raise_on(st)
    return _copy_and_free(out[], outlen[])
end

# ---------------------------------------------------------------------------
# stacked_bar
# ---------------------------------------------------------------------------

function stacked_bar(series, w, h;
                     colors=nothing, bg_color=nothing, cat_labels=nothing,
                     show_labels::Bool=false, show_prices::Bool=false,
                     plain::Bool=false)
    series = collect(series)
    isempty(series) && throw(CchartsError(1, "need at least one series"))
    cats = length(_series_values(first(series)))
    keep = Any[]
    arr = KStackSeries[]
    sizehint!(arr, length(series))
    for sr in series
        name = _series_name(sr)
        vals = Float64.(_series_values(sr))
        length(vals) == cats || throw(CchartsError(1, "all series must have the same number of values"))
        vp = pointer(vals); push!(keep, vals)
        push!(arr, KStackSeries(_cstr(name, keep), vp))
    end
    push!(keep, arr)

    # NULL-terminated per-series color pointer array.
    color_ptrs = Ptr{UInt8}[]
    if plain
        for _ in 1:length(series); push!(color_ptrs, _cstr("", keep)); end
        push!(color_ptrs, NUL)
        colors_ptr = pointer(color_ptrs); push!(keep, color_ptrs)
    elseif colors !== nothing && !isempty(colors)
        (colors_ptr, _) = _ptr_array(colors, keep; terminate=true)
    else
        colors_ptr = NUL
    end

    (cat_ptr, _) = _ptr_array(cat_labels, keep)

    s = CStackSettings(colors_ptr, _color(bg_color, keep; plain), cat_ptr,
                       Cint(length(series)), Cint(cats),
                       Cint(show_labels ? 1 : 0), Cint(show_prices ? 1 : 0))
    out = Ref{Ptr{UInt8}}(C_NULL); outlen = Ref{Csize_t}(0)
    st = ccall((:ccharts_stack, _LIB), Cint,
               (Ptr{KStackSeries}, Cint, Cint, Cint, Ref{CStackSettings},
                Ref{Ptr{UInt8}}, Ref{Csize_t}),
               pointer(arr), Cint(length(arr)), Cint(w), Cint(h), Ref(s), out, outlen)
    _raise_on(st)
    return _copy_and_free(out[], outlen[])
end

_series_name(sr) = begin
    if sr isa Pair
        string(first(sr))
    elseif sr isa Tuple && length(sr) >= 2
        string(sr[1])
    elseif sr isa AbstractDict
        haskey(sr, :name) ? string(sr[:name]) :
        haskey(sr, "name") ? string(sr["name"]) : ""
    elseif hasproperty(sr, :name)
        string(getproperty(sr, :name))
    else
        ""
    end
end

_series_values(sr) = begin
    if sr isa Pair
        last(sr)
    elseif sr isa Tuple && length(sr) >= 2
        sr[2]
    elseif sr isa AbstractDict
        haskey(sr, :values) ? sr[:values] :
        haskey(sr, "values") ? sr["values"] :
        haskey(sr, :samples) ? sr[:samples] :
        haskey(sr, "samples") ? sr["samples"] :
        throw(CchartsError(1, "series has no values"))
    elseif hasproperty(sr, :values)
        getproperty(sr, :values)
    elseif hasproperty(sr, :samples)
        getproperty(sr, :samples)
    else
        throw(CchartsError(1, "series has no values"))
    end
end

# ---------------------------------------------------------------------------
# heatmap
# ---------------------------------------------------------------------------

function heatmap(values, w, h;
                 low_color=nothing, high_color=nothing, mid_color=nothing,
                 bg_color=nothing, row_labels=nothing, col_labels=nothing,
                 show_labels::Bool=false, plain::Bool=false)
    values = [collect(row) for row in values]
    isempty(values) && throw(CchartsError(1, "need a non-empty value matrix"))
    cols = length(values[1])
    cols == 0 && throw(CchartsError(1, "matrix columns must not be empty"))
    for row in values
        length(row) == cols || throw(CchartsError(1, "all rows must have the same number of values"))
    end
    keep = Any[]
    flat = Float64[]
    sizehint!(flat, length(values) * cols)
    for row in values; append!(flat, Float64.(row)); end
    fp = pointer(flat); push!(keep, flat)

    (rlab_ptr, _) = _ptr_array(row_labels, keep)
    (clab_ptr, _) = _ptr_array(col_labels, keep)

    s = CHeatSettings(_color(low_color, keep; plain), _color(high_color, keep; plain),
                      _color(mid_color, keep; plain), _color(bg_color, keep; plain),
                      rlab_ptr, clab_ptr, Cint(show_labels ? 1 : 0))
    out = Ref{Ptr{UInt8}}(C_NULL); outlen = Ref{Csize_t}(0)
    st = ccall((:ccharts_heat, _LIB), Cint,
               (Ptr{Float64}, Cint, Cint, Cint, Cint, Ref{CHeatSettings},
                Ref{Ptr{UInt8}}, Ref{Csize_t}),
               fp, Cint(length(values)), Cint(cols), Cint(w), Cint(h),
               Ref(s), out, outlen)
    _raise_on(st)
    return _copy_and_free(out[], outlen[])
end

# ---------------------------------------------------------------------------
# boxplot
# ---------------------------------------------------------------------------

function boxplot(categories, w, h;
                 rise_color=nothing, area_color=nothing, bg_color=nothing,
                 show_prices::Bool=false, plain::Bool=false)
    categories = collect(categories)
    isempty(categories) && throw(CchartsError(1, "need at least one category"))
    keep = Any[]
    arr = KBoxCategory[]
    sizehint!(arr, length(categories))
    for cat in categories
        name = _series_name(cat)
        smp = Float64.(_series_values(cat))
        isempty(smp) && throw(CchartsError(1, "every category must have at least one sample"))
        sp = pointer(smp); push!(keep, smp)
        push!(arr, KBoxCategory(_cstr(name, keep), sp, Cint(length(smp))))
    end
    push!(keep, arr)
    s = CBoxSettings(_color(rise_color, keep; plain), _color(area_color, keep; plain),
                     _color(bg_color, keep; plain), Cint(show_prices ? 1 : 0))
    out = Ref{Ptr{UInt8}}(C_NULL); outlen = Ref{Csize_t}(0)
    st = ccall((:ccharts_box, _LIB), Cint,
               (Ptr{KBoxCategory}, Cint, Cint, Cint, Ref{CBoxSettings},
                Ref{Ptr{UInt8}}, Ref{Csize_t}),
               pointer(arr), Cint(length(arr)), Cint(w), Cint(h), Ref(s), out, outlen)
    _raise_on(st)
    return _copy_and_free(out[], outlen[])
end

end # module