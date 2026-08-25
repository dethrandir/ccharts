# test/test_conformance.jl — the shared 70-case conformance suite.
#
# Every case in ../../conformance/cases.json is rendered through the public
# Ccharts API and compared byte-for-byte against ../../conformance/golden/<name>.txt.
# A settings/struct mismatch shows up as a byte difference, so this is the
# acceptance gate for the binding.
using Test
using Ccharts
using JSON

CONF_DIR = normpath(joinpath(@__DIR__, "..", "..", "..", "conformance"))

# cfg field accessor that treats JSON null (Julia `nothing`) as the default.
_cfg(cfg, k, dflt) = haskey(cfg, k) && cfg[k] !== nothing ? cfg[k] : dflt

function _floats(v)
    v === nothing ? nothing : Float64.(v)
end

function render_case(doc, case_data)
    cfg = get(case_data, "settings", Dict())
    chart = case_data["chart"]
    w = case_data["width"]; h = case_data["height"]

    if chart == "hist"
        samples = Float64.(case_data["samples"])
        return histogram(samples, w, h;
            rise_color=_cfg(cfg,"rise_color",nothing),
            bg_color=_cfg(cfg,"bg_color",nothing),
            bin_count=_cfg(cfg,"bin_count",0),
            min_value=_cfg(cfg,"min_value",NaN),
            max_value=_cfg(cfg,"max_value",NaN),
            show_bins=_cfg(cfg,"show_bins",false),
            show_prices=_cfg(cfg,"show_prices",false),
            plain=_cfg(cfg,"plain",false))

    elseif chart == "spark"
        samples = Float64.(case_data["samples"])
        return sparkline(samples, w, h;
            rise_color=_cfg(cfg,"rise_color",nothing),
            area_color=_cfg(cfg,"area_color",nothing),
            min_above=_cfg(cfg,"min_above",0),
            min_below=_cfg(cfg,"min_below",0),
            plain=_cfg(cfg,"plain",false))

    elseif chart == "bar"
        labels = [string(it["label"]) for it in case_data["items"]]
        values = Float64.([it["value"] for it in case_data["items"]])
        return bar(collect(zip(labels, values)), w, h;
            rise_color=_cfg(cfg,"rise_color",nothing),
            bg_color=_cfg(cfg,"bg_color",nothing),
            show_labels=_cfg(cfg,"show_labels",false),
            show_prices=_cfg(cfg,"show_prices",false),
            plain=_cfg(cfg,"plain",false))

    elseif chart == "pie"
        slices = [(string(sl["label"]), Float64(sl["value"])) for sl in case_data["slices"]]
        return pie(slices, w, h;
            donut=_cfg(cfg,"donut",false),
            colors=_cfg(cfg,"colors",nothing),
            show_legend=_cfg(cfg,"show_legend",true),
            show_pct=_cfg(cfg,"show_pct",false),
            slice_gap=_cfg(cfg,"slice_gap",0.0),
            inner_radius_ratio=_cfg(cfg,"inner_radius_ratio",-1.0),
            legend_format=_cfg(cfg,"legend_format",0),
            start_angle=_cfg(cfg,"start_angle",-1.0),
            counter_clockwise=_cfg(cfg,"counter_clockwise",false),
            center_text=_cfg(cfg,"center_text",nothing))

    elseif chart == "stack"
        series = [(string(sr["name"]), Float64.(sr["values"])) for sr in case_data["series"]]
        return stacked_bar(series, w, h;
            colors=_cfg(cfg,"colors",nothing),
            bg_color=_cfg(cfg,"bg_color",nothing),
            cat_labels=_cfg(cfg,"cat_labels",nothing),
            show_labels=_cfg(cfg,"show_labels",false),
            show_prices=_cfg(cfg,"show_prices",false),
            plain=_cfg(cfg,"plain",false))

    elseif chart == "heat"
        matrix = [Float64.(row) for row in case_data["values"]]
        return heatmap(matrix, w, h;
            low_color=_cfg(cfg,"low_color",nothing),
            high_color=_cfg(cfg,"high_color",nothing),
            mid_color=_cfg(cfg,"mid_color",nothing),
            bg_color=_cfg(cfg,"bg_color",nothing),
            row_labels=_cfg(cfg,"row_labels",nothing),
            col_labels=_cfg(cfg,"col_labels",nothing),
            show_labels=_cfg(cfg,"show_labels",false),
            plain=_cfg(cfg,"plain",false))

    elseif chart == "box"
        categories = [(string(c["name"]), Float64.(c["samples"])) for c in case_data["categories"]]
        return boxplot(categories, w, h;
            rise_color=_cfg(cfg,"rise_color",nothing),
            area_color=_cfg(cfg,"area_color",nothing),
            bg_color=_cfg(cfg,"bg_color",nothing),
            show_prices=_cfg(cfg,"show_prices",false),
            plain=_cfg(cfg,"plain",false))

    else # line / candle
        dataset = doc["datasets"][case_data["dataset"]]
        c = if get(case_data,"source","arrays") == "json"
            Chart.from_json(dataset["json"])
        else
            ts = isempty(dataset) || !haskey(dataset,"ts") ? nothing : Int64[round(Int,x) for x in dataset["ts"]]
            Chart.from_arrays(_floats(dataset["open"]), _floats(dataset["high"]),
                              _floats(dataset["low"]), _floats(dataset["close"]); ts=ts)
        end
        return (chart == "line" ? line : candle)(c, w, h;
            rise_color=_cfg(cfg,"rise_color",nothing),
            fall_color=_cfg(cfg,"fall_color",nothing),
            bg_color=_cfg(cfg,"bg_color",nothing),
            area_color=_cfg(cfg,"area_color",nothing),
            single_color=_cfg(cfg,"single_color",false),
            show_prices=_cfg(cfg,"show_prices",false),
            show_times=_cfg(cfg,"show_times",false),
            plain=_cfg(cfg,"plain",false))
    end
end

@testset "all 70 conformance cases match the goldens byte-for-byte" begin
    cases_file = joinpath(CONF_DIR, "cases.json")
    @test isfile(cases_file)
    doc = JSON.parsefile(cases_file)
    cases = doc["cases"]
    @test length(cases) >= 70  # truncation guard

    failures = String[]
    for case_data in cases
        name = case_data["name"]
        rendered = render_case(doc, case_data)
        expected_path = joinpath(CONF_DIR, "golden", name * ".txt")
        @test isfile(expected_path)
        expected = read(expected_path, String)
        rendered == expected || push!(failures, name)
    end
    @test isempty(failures)
    if !isempty(failures)
        println("conformance mismatches: ", failures)
    end
    @test length(cases) == 70
end