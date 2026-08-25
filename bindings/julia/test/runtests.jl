# test/runtests.jl — package entry: API smoke tests + full 70-case conformance.
using Test
using Ccharts

@testset "Ccharts" begin
    @testset "API" begin
        include("test_api.jl")
    end
    @testset "Conformance" begin
        include("test_conformance.jl")
    end
end