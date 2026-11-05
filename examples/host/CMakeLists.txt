# =============================================================================
# XMPL100 driver: host example (any PC, no hardware)
# =============================================================================
# The driver against a simulated XMPL100 (the register model the host tests
# use, tests/mock_xmpl100.hpp) through the host port in this folder.
#
#   cmake -S examples/host -B build-example
#   cmake --build build-example && ./build-example/xmpl100_host_example
#
# The host tests also build this folder and run the example (CTest
# xmpl100_host_example), so it always compiles against the current headers.
# =============================================================================
cmake_minimum_required(VERSION 3.16)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  project(xmpl100_host_example LANGUAGES CXX)
  add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../.. xmpl100)
endif()

add_executable(xmpl100_host_example main.cpp)
target_link_libraries(xmpl100_host_example PRIVATE xmpl100::xmpl100)
target_include_directories(xmpl100_host_example PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/../../tests)
target_compile_features(xmpl100_host_example PRIVATE cxx_std_20)
target_compile_options(xmpl100_host_example PRIVATE -fno-exceptions -fno-rtti -Wall -Wextra -Wpedantic)
