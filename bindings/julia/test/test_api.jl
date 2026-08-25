# test/test_api.jl — API smoke tests (independent of the conformance goldens).
using Test
using Ccharts

@testset "dataset + line/candle" begin
    c = Chart.from_arrays([1,2,3], [2,3,4], [0,1,2], [1.5,2.5,3.5])
    @test size(c) == 3
    l = line(c, 40, 8)
    @test l isa String
    @test !isempty(l)
    cd = candle(c, 40, 8)
    @test cd isa String && !isempty(cd)

    d = Chart.from_arrays([1,2,3], [2,3,4], [0,1,2], [1.5,2.5,3.5]; ts=[100,200,300])
    @test size(d) == 3
    @test line(d, 40, 8; show_prices=true, show_times=true) isa String
    @test line(c, 40, 8; rise_color="red", plain=true) isa String
end

@testset "json / csv" begin
    j = Chart.from_json("[{\"open\":328.75,\"high\":330.0,\"low\":323.75,\"close\":328.0},{\"open\":330.0,\"high\":330.25,\"low\":317.5,\"close\":317.5}]")
    @test size(j) == 2
    csv = "328.75,330.0,323.75,328.0,1784505600\n330.0,330.25,317.5,317.5,1784592000\n"
    c = Chart.from_csv(csv)
    @test size(c) == 2
end

@testset "standalone renderers" begin
    @test pie([("A",40),("B",30),("C",30)], 24, 10; show_legend=true, show_pct=true) isa String
    @test pie([(:Alpha,40),(:Beta,30)], 24, 10; donut=true) isa String
    @test histogram([1,2,2,3,3,3,4,4,5,6], 40, 8) isa String
    @test histogram([1,2,3,4], 40, 8; min_value=0.0, max_value=5.0, show_bins=true) isa String
    @test sparkline([5,7,4,8,6,9,4,7,10], 24, 1) isa String
    @test bar([("A",1),("B",4),("C",2)], 40, 8; show_labels=true) isa String
    @test stacked_bar([("Alpha",[1,4,2,5,3]),("Beta",[3,2,5,1,4])], 40, 8) isa String
    @test stacked_bar([("Alpha",[1,2,3]),("Beta",[3,2,1])], 40, 8; cat_labels=["a","b","c"], show_labels=true) isa String
    @test heatmap([[0.0,0.2,0.4],[0.6,0.8,1.0]], 5, 5) isa String
    @test heatmap([[0.0,0.1],[0.9,1.0]], 4, 4; row_labels=["a","b"], col_labels=["c","d"], show_labels=true) isa String
    @test boxplot([("A",[1,4,2,5,3]),("B",[1,2,3,4,5,6,7,8,9])], 40, 8) isa String
end

@testset "introspection" begin
    @test version() == "3.0.0"
    @test max_dim() == 100000
    @test max_cells() > 0
    @test color(1) == "\e[31m"   # red
    @test color(999) === nothing
end

chart_empty_err() = begin
    c = Chart.from_arrays([1,2], [2,3], [1,2], [1.5,2.5])
    line(c, 0, 8)  # width 0 -> CCHARTS_ERR_DIMENSIONS
end

@testset "errors" begin
    @test_throws CchartsError Chart.from_arrays([1], [2], [3, 4], [4])  # unequal lengths
    @test_throws CchartsError chart_empty_err()
end