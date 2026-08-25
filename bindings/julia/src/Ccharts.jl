### src/Ccharts.jl — Julia binding for the ccharts single-header C library.
#
# Links the shared library produced by deps/build.jl (gcc over the vendored
# abi/ccharts_abi.c) through a ccall FFI. Struct packing mirrors the C layout
# field-for-field; contiguous struct arrays (pie/bar/stack/box) are Julia
# `Array{<:isbits struct}` blocks, which are laid out as one contiguous, C-ABI
# aligned run of `size`-byte elements — never an array of pointers.
module Ccharts

using Libdl

# Locate the built native library (deps/deps.jl), with CCHARTS_NATIVE_DIR
# fallback. Build failure here points back at `Pkg.build("Ccharts")`.
include("../deps/deps.jl")
const _LIB = Ccharts_deps.CCHARTS_LIBPATH

# ---------------------------------------------------------------------------
# Error type
# ---------------------------------------------------------------------------

"""Error raised on any non-CCHARTS_OK status from the C layer."""
struct CchartsError <: Exception
    status::Int32
    message::String
end
Base.showerror(io::IO, e::CchartsError) =
    print(io, "CchartsError(", e.status, "): ", e.message)

function _raise_on(status::Integer)
    status == 0 && return status
    msgptr = ccall((:ccharts_error_message, _LIB), Ptr{UInt8}, (Cint,), Cint(status))
    msg = msgptr == C_NULL ? "" : unsafe_string(msgptr)
    throw(CchartsError(status, msg))
end

# ---------------------------------------------------------------------------
# C settings structs (layouts from abi/ccharts_abi.h). Julia lays out isbits
# structs in C-ABI order with standard alignment/padding, so these match the
# C side byte-for-byte. All pointers are raw addresses we keep rooted via a
# `keep` vector across the render call.
# ---------------------------------------------------------------------------

# ccharts_settings (line / candle)
struct CSettings
    rise_color::Ptr{UInt8}    # UInt8 == Char in C, pointer
    fall_color::Ptr{UInt8}
    bg_color::Ptr{UInt8}
    area_color::Ptr{UInt8}
    single_color::Int32
    show_prices::Int32
    show_times::Int32
end

# ccharts_pie_slice
struct KPieSlice
    label::Ptr{UInt8}
    value::Float64
end

# ccharts_hist_settings
struct CHistSettings
    rise_color::Ptr{UInt8}
    bg_color::Ptr{UInt8}
    bin_count::Int32
    min_value::Float64
    max_value::Float64
    show_bins::Int32
    show_prices::Int32
end

# ccharts_spark_settings
struct CSparkSettings
    rise_color::Ptr{UInt8}
    area_color::Ptr{UInt8}
    min_above::Int32
    min_below::Int32
end

# ccharts_bar_slice
struct KBarSlice
    label::Ptr{UInt8}
    value::Float64
end

# ccharts_bar_settings
struct CBarSettings
    rise_color::Ptr{UInt8}
    bg_color::Ptr{UInt8}
    show_labels::Int32
    show_prices::Int32
end

# ccharts_stack_series
struct KStackSeries
    name::Ptr{UInt8}
    values::Ptr{Float64}
end

# ccharts_stack_settings
struct CStackSettings
    colors::Ptr{UInt8}
    bg_color::Ptr{UInt8}
    cat_labels::Ptr{UInt8}
    series::Int32
    cats::Int32
    show_labels::Int32
    show_prices::Int32
end

# ccharts_heat_settings
struct CHeatSettings
    low_color::Ptr{UInt8}
    high_color::Ptr{UInt8}
    mid_color::Ptr{UInt8}
    bg_color::Ptr{UInt8}
    row_labels::Ptr{UInt8}
    col_labels::Ptr{UInt8}
    show_labels::Int32
end

# ccharts_box_category
struct KBoxCategory
    name::Ptr{UInt8}
    samples::Ptr{Float64}
    n::Int32
end

# ccharts_box_settings
struct CBoxSettings
    rise_color::Ptr{UInt8}
    area_color::Ptr{UInt8}
    bg_color::Ptr{UInt8}
    show_prices::Int32
end

# ---------------------------------------------------------------------------
# Helpers: rooted C strings + color escapes + pointer arrays.
# ---------------------------------------------------------------------------

const NUL = C_NULL  # Ptr{UInt8}(0)

# Copy a Julia string into a NUL-terminated byte buffer and root it in `keep`,
# returning the address. `nothing` -> NULL (library default).
function _cstr(s, keep)
    b = Vector{UInt8}(codeunits(s))
    push!(b, 0x00)
    p = pointer(b)
    push!(keep, b)
    return p
end

# Resolve a color setting: `nothing` -> library default (NULL); a name/escape
# -> its ANSI escape bytes; `plain=true` forces an *empty* escape so no ANSI
# is emitted at all (overriding even an explicit color).
const _ESCAPES = Dict{String,String}(
    "black"         => "\e[30m",
    "red"           => "\e[31m",
    "green"         => "\e[32m",
    "yellow"        => "\e[33m",
    "blue"          => "\e[34m",
    "magenta"       => "\e[35m",
    "cyan"          => "\e[36m",
    "white"         => "\e[37m",
    "bright_black"  => "\e[90m",
    "bright_red"    => "\e[91m",
    "bright_green"  => "\e[92m",
    "bright_yellow" => "\e[93m",
    "bright_blue"   => "\e[94m",
    "bright_magenta"=> "\e[95m",
    "bright_cyan"   => "\e[96m",
    "bright_white"  => "\e[97m",
    "reset"         => "\e[0m",
)

function _color(value, keep; plain=false)
    plain && return _cstr("", keep)         # no ANSI at all
    value === nothing && return NUL
    s = string(value)
    isempty(s) && return NUL                # empty -> library default
    _cstr(get(_ESCAPES, s, s), keep)        # name, or pass raw escape through
end

# Build a contiguous array of NUL-terminated escapes/labels (used for
# pie colors, stack colors/labels, heat labels). `terminate` appends a 0
# sentinel (stack colors). `plain` resolves like _color.
function _ptr_array(values, keep; terminate=false, plain=false)
    ptrs = Ptr{UInt8}[]
    if values === nothing
        return (NUL, ptrs)
    end
    for v in values
        push!(ptrs, _color(v, keep; plain))
    end
    terminate && push!(ptrs, NUL)
    block = Vector{Ptr{UInt8}}(ptrs)
    push!(keep, block)
    return (pointer(block), ptrs)
end

# ---------------------------------------------------------------------------
# Introspection (public)
# ---------------------------------------------------------------------------

"""Library version ("3.0.0")."""
function version()
    p = ccall((:ccharts_version, _LIB), Ptr{UInt8}, ())
    p == C_NULL ? "" : unsafe_string(p)
end

"""ANSI escape for a ccharts color index, or `nothing` when out of range."""
function color(index::Integer)
    p = ccall((:ccharts_color, _LIB), Ptr{UInt8}, (Cint,), Cint(index))
    p == C_NULL ? nothing : unsafe_string(p)
end

max_dim()    = ccall((:ccharts_max_dim, _LIB), Cint, ())
max_cells()  = ccall((:ccharts_max_cells, _LIB), Cint, ())

include("Chart.jl")

# Re-export the public surface defined in the Chart submodule. `line`/`candle`
# also work as instance methods (chart.line(w, h); ...), and the standalone
# renderers + dataset constructors are module-level functions.
using .__CchartsChart__: Chart,
    from_arrays, from_json, from_csv,
    line, candle,
    pie, histogram, sparkline, bar, stacked_bar, heatmap, boxplot

export Chart,
       from_arrays, from_json, from_csv,
       line, candle,
       pie, histogram, sparkline, bar, stacked_bar, heatmap, boxplot

export CchartsError, version, color, max_dim, max_cells

end # module