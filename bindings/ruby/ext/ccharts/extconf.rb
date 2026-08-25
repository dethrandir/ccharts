# frozen_string_literal: true

# Builds the ccharts native library (ccharts_ext.so) from the vendored C ABI.
#
# The extension declares no Ruby-facing functions of its own — it is plain
# object code (the ccharts_* symbols) that Fiddle dlopens, so the C compiler
# just needs to turn vendor/ccharts_abi.c into a shared object. This keeps the
# gem compile-from-source and platform-portable: any C compiler at install time
# produces a working FFI library, exactly like the Rust/Go bindings.
require "mkmf"

# ccharts.h uses math functions (lround, etc.), so link libm on non-Windows.
unless RUBY_PLATFORM =~ /mswin|mingw|cygwin/
  $LDFLAGS << " -lm"
end

# The C ABI lives in vendor/; point mkmf's source directory there so
# vendor/ccharts_abi.c becomes the extension's object code. The .so is still
# written to this directory (ext/ccharts/).
create_makefile("ccharts_ext", "vendor")
