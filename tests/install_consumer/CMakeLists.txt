# A project outside the source tree that uses only the installed package:
# find_package(xmpl100) and xmpl100::xmpl100. Run by tests/check_install.cmake.
cmake_minimum_required(VERSION 3.16)
project(xmpl100_install_consumer LANGUAGES CXX)

find_package(xmpl100 0.1 REQUIRED CONFIG)

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE xmpl100::xmpl100)
target_compile_options(consumer PRIVATE -fno-exceptions -fno-rtti)
