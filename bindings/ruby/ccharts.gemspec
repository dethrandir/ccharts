# frozen_string_literal: true

require_relative "lib/ccharts/version"

Gem::Specification.new do |spec|
  spec.name          = "ccharts"
  spec.version       = Ccharts::VERSION
  spec.authors       = ["ccharts contributors"]
  spec.summary       = "Financial OHLC data as a string — line, candle, pie, histogram, sparkline, bar, stacked-bar, heatmap and box-plot charts drawn with Unicode block characters and optional ANSI color."
  spec.description   = "A Ruby binding to the ccharts single-header C library. Turns OHLC / sample data into text charts (line, candle, pie/donut, histogram, sparkline, bar, stacked bar, heatmap, box plot). The vendored C ABI is compiled from source by mkmf and driven through Fiddle, so there are no runtime dependencies beyond the Ruby standard library."
  spec.homepage      = "https://github.com/dethrandir/ccharts"
  spec.license       = "MIT"

  spec.required_ruby_version = ">= 2.7.0"

  spec.files = Dir["lib/**/*.rb"] +
               Dir["ext/ccharts/vendor/*.{h,c}"] +
               ["ext/ccharts/extconf.rb"]
  spec.require_paths = ["lib"]

  # Compile the vendored C ABI into ccharts_ext.so at install time.
  spec.extensions = ["ext/ccharts/extconf.rb"]

  spec.add_development_dependency "rake", "~> 13.0"

  spec.metadata["rubygems_mfa_required"] = "true"
end
