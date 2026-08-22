# Build for the flat C ABI (abi/ccharts_abi.c) that every non-Python binding
# links against. Rust and Go do not use this file — they compile the same .c
# with their own toolchains (cc crate / cgo). It exists for the C# and Java
# prebuilt native matrix, for C consumers who want a real library, and for
# local development.
#
# Targets:
#   ccharts_abi         shared library (libccharts_abi.so/.dylib/.dll)
#   ccharts_abi_static  static library
#   ccharts_smoke       links the static library and renders one chart
cmake_minimum_required(VERSION 3.15)

project(ccharts C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Only CCHARTS_API symbols leave the shared library; everything ccharts.h
# defines is static anyway, but this keeps the export table honest.
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(ccharts_abi SHARED abi/ccharts_abi.c)
add_library(ccharts_abi_static STATIC abi/ccharts_abi.c)

foreach(target ccharts_abi ccharts_abi_static)
  target_include_directories(${target} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/abi>)
  if(MSVC)
    # The block characters are \uXXXX universal character names; without
    # /utf-8 MSVC converts them to the system ANSI codepage and the
    # unrepresentable glyphs degrade to '?' at runtime (same reason setup.py
    # passes it for the Python extension).
    target_compile_options(${target} PRIVATE /utf-8 /W3)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra)
    target_link_libraries(${target} PRIVATE m)
  endif()
endforeach()

target_compile_definitions(ccharts_abi PRIVATE CCHARTS_ABI_BUILD_SHARED)
set_target_properties(ccharts_abi PROPERTIES OUTPUT_NAME ccharts_abi)
set_target_properties(ccharts_abi_static PROPERTIES OUTPUT_NAME ccharts_abi_static)

add_executable(ccharts_smoke conformance/smoke.c)
target_link_libraries(ccharts_smoke PRIVATE ccharts_abi_static)

include(CTest)
if(BUILD_TESTING)
  add_test(NAME smoke COMMAND ccharts_smoke)
endif()

install(TARGETS ccharts_abi ccharts_abi_static
        RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
install(FILES abi/ccharts_abi.h ccharts.h DESTINATION include)
