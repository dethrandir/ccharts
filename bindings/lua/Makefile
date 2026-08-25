# Makefile for the ccharts Lua binding (dev build, no LuaRocks required).
#
# Builds the compiled C module against the vendored C ABI and the Lua headers
# of the interpreter you will load it into. LuaRocks does exactly the same via
# the rockspec; this just makes the code verifiable with plain `lua` when
# LuaRocks or a matching dev package is unavailable.
#
# Two module names are produced from the single C source (which exports both
# luaopen_* symbols): build/ccharts_core.so (loaded by the pure-Lua facade,
# src/ccharts.lua) and src/ccharts.so (so the top-level `require "ccharts"`
# loads the compiled module directly).
#
#   make            # build both modules
#   make test       # build + run test_api.lua + test_conformance.lua

LUA           ?= lua
CC            ?= cc
LUA_INCDIR    ?= ../../build/lua54/include
LUA_LIBDIR    ?= /usr/lib64
LUA_LIB       ?= lua-5.4

CFLAGS        ?= -O2 -fPIC -std=gnu99 -Wall -Wextra -I$(LUA_INCDIR) -Ivendor
LDFLAGS       ?= -shared -L$(LUA_LIBDIR) -Wl,-rpath,$(LUA_LIBDIR)
LDLIBS        ?= -l$(LUA_LIB) -lm

BUILD         := build

.PHONY: all clean test

all: $(BUILD)/ccharts_core.so src/ccharts.so

$(BUILD)/ccharts_core.so: src/ccharts_core.c vendor/ccharts_abi.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ src/ccharts_core.c vendor/ccharts_abi.c $(LDLIBS)

src/ccharts.so: src/ccharts_core.c vendor/ccharts_abi.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ src/ccharts_core.c vendor/ccharts_abi.c $(LDLIBS)

test: all
	$(LUA) -e 'package.cpath="./src/?.so;"..package.cpath; local c=require "ccharts"; assert(c.version=="0.2.2","version mismatch"); print("ccharts version: "..c.version)'
	$(LUA) tests/test_api.lua
	$(LUA) tests/test_conformance.lua

clean:
	rm -rf $(BUILD)/ccharts_core.so src/ccharts.so