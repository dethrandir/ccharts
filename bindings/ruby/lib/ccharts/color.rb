# frozen_string_literal: true

module Ccharts
  # ANSI color escapes matching the C library's ccharts_color_index values
  # (abi/ccharts_abi.h). These come straight from the library: a binding passes
  # the escape string (or NULL for the library default) through the settings
  # structs, so every binding resolves the same names to the same escapes.
  module Color
    ESCAPES = {
      "black"        => "\e[30m",
      "red"          => "\e[31m",
      "green"        => "\e[32m",
      "yellow"       => "\e[33m",
      "blue"         => "\e[34m",
      "magenta"      => "\e[35m",
      "cyan"         => "\e[36m",
      "white"        => "\e[37m",
      "bright_black" => "\e[90m",
      "bright_red"   => "\e[91m",
      "bright_green" => "\e[92m",
      "bright_yellow"=> "\e[93m",
      "bright_blue"  => "\e[94m",
      "bright_magenta"=> "\e[95m",
      "bright_cyan"  => "\e[96m",
      "bright_white" => "\e[97m",
      "reset"        => "\e[0m",
    }.freeze

    # Resolve a color given either a name ("blue"), a Symbol (:blue), or a raw
    # ANSI escape sequence (256-color/truecolor strings passed straight through).
    # Returns the escape string, or nil if the value is nil/empty (library
    # default). Unknown names fall back to the literal string so raw escapes
    # keep working.
    def self.resolve(value)
      return nil if value.nil?
      s = value.to_s
      return nil if s.empty?
      ESCAPES[s] || s
    end

    # The ANSI escape for a named color, or nil when unknown.
    def self.[](name)
      ESCAPES[name.to_s]
    end
  end
end
