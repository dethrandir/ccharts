# deps/build.jl — compile the vendored ccharts ABI into a shared library.
#
# The binding follows the compile-from-source family (Ruby mkmf, Lua package,
# Rust/Go/cgo): Pkg.build runs `gcc` against bindings/julia/vendor/ccharts_abi.c
# and produces deps/ccharts.<libext>. The package links it via ccall. A C
# compiler is therefore required at build time (the same tradeoff as the
# Ruby/Lua bindings). A CCHARTS_CC environment variable selects the compiler,
# defaulting to gcc.

const deps_dir = joinpath(@__DIR__)
const vendor_dir = joinpath(@__DIR__, "..", "vendor")

function libext()
    Sys.iswindows() ? "dll" :
    Sys.isapple()  ? "dylib" :
                      "so"
end

src = joinpath(vendor_dir, "ccharts_abi.c")
for f in ("ccharts_abi.c", "ccharts_abi.h", "ccharts.h")
    if !isfile(joinpath(vendor_dir, f))
        error("missing vendored source: $(joinpath(vendor_dir, f)). " *
              "Run `python3 scripts/sync_sources.py` at the repo root to populate bindings/julia/.")
    end
end
isfile(src) || error("missing $(src)")

out = joinpath(deps_dir, "ccharts.$(libext())")

# Rebuild only when the source or build script is newer than the artifact.
stamp = joinpath(deps_dir, "build.stamp")
if isfile(out) && isfile(stamp) && mtime(out) >= mtime(src) && mtime(out) >= mtime(stamp)
    @info "ccharts native library already built ($out)"
else
    cc = get(ENV, "CCHARTS_CC", "gcc")
    cmd = `$cc -shared -fPIC -O2 -fvisibility=hidden -o $out $src -lm`
    @info "compiling ccharts ABI -> $out" cmd
    run(cmd)
    open(stamp, "w") do io end
    @info "built ccharts native library ($out)"
end