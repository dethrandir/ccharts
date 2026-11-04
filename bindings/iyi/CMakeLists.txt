# Build the existing flat ABI; the iyi layer contains no rendering logic.
cmake_minimum_required(VERSION 3.15)
project(ccharts_iyi C)
add_library(ccharts STATIC vendor/ccharts_abi.c)
target_include_directories(ccharts PRIVATE vendor)
set_target_properties(ccharts PROPERTIES
  C_STANDARD 99 C_STANDARD_REQUIRED ON POSITION_INDEPENDENT_CODE ON
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
  ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}"
  ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}")
if(MSVC)
  target_compile_options(ccharts PRIVATE /utf-8 /W3)
else()
  target_compile_options(ccharts PRIVATE -Wall -Wextra -Werror -pedantic)
endif()
