# deps/deps.jl — locate the built ccharts native library.
#
# deps/build.jl compiles bindings/julia/vendor/ccharts_abi.c into a shared
# object straight into this directory (deps/ccharts.<libext>). This file just
# resolves that path at load time, with a CCHARTS_NATIVE_DIR fallback for
# environments that ship a prebuilt library.
module Ccharts_deps

libext() =
    Sys.iswindows() ? "dll" :
    Sys.isapple()  ? "dylib" :
                      "so"

function find_library()
    here = @__DIR__
    local_path = joinpath(here, "ccharts.$(libext())")
    if isfile(local_path)
        return local_path
    end
    dir = get(ENV, "CCHARTS_NATIVE_DIR", "")
    if !isempty(dir)
        cand = joinpath(dir, "ccharts.$(libext())")
        isfile(cand) && return cand
    end
    error("ccharts native library not found. Run `Pkg.build(\"Ccharts\")` (or set CCHARTS_NATIVE_DIR) first.")
end

const CCHARTS_LIBPATH = find_library()

end # module