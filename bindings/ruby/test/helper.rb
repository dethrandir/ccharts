# frozen_string_literal: true

# Load the library and provide a tiny, dependency-free test harness so the
# suite runs with plain `ruby` (no minitest/rake required to run the tests).

$LOAD_PATH.unshift File.expand_path("../lib", __dir__)
require "ccharts"
require "json"

module TinyTest
  @tests = []

  def self.test(name, &blk)
    @tests << [name, blk]
  end

  def self.run(pattern = //)
    puts "== #{caller_locations(1, 1)[0].path} =="
    ran = 0
    failed = []
    @tests.each do |name, blk|
      next unless name =~ pattern
      ran += 1
      begin
        blk.call
        puts "PASS  #{name}"
      rescue StandardError => e
        failed << name
        puts "FAIL  #{name}: #{e.class}: #{e.message}"
        puts e.backtrace.first(6).map { |l| "        #{l}" }
      end
    end
    puts "#{ran - failed.length}/#{ran} tests passed"
    exit(1) unless failed.empty?
  end
end

def test(name, &blk)
  TinyTest.test(name, &blk)
end

def assert(cond, msg = "assertion failed")
  raise msg unless cond
end

def assert_equal(expected, actual, msg = nil)
  return if expected == actual
  raise "expected #{expected.inspect}, got #{actual.inspect}#{": #{msg}" if msg}"
end

def assert_raises(klass)
  yield
  raise "expected #{klass} to be raised, but nothing was"
rescue klass
  nil
end

# Conformance helpers --------------------------------------------------------

def conformance_dir
  File.expand_path("../../../conformance", __dir__)
end

A_2 = ->(h, k) { h[k] }
