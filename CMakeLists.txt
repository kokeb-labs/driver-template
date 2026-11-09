#===============================================================================
# XMPL100 driver: root CMake
#===============================================================================
# scaffold:platform-esp32-header
# One file, two build systems:
#
# - ESP-IDF (ESP_PLATFORM): this folder is an ESP-IDF component. Point
#   EXTRA_COMPONENT_DIRS at it, check it out under components/, or add it as a
#   component-manager git dependency. menuconfig reads _config/Kconfig.
# /scaffold:platform-esp32-header
# - Any other CMake build (host, STM32, RP2040, nRF, ...): one INTERFACE target,
#   xmpl100, consumed as xmpl100::xmpl100 both through add_subdirectory() and
#   through find_package(xmpl100) after install.
#
#   cmake -S . -B build -DXMPL100_BUILD_TESTS=ON
#   cmake --build build && ctest --test-dir build --output-on-failure
#===============================================================================
cmake_minimum_required(VERSION 3.16)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/xmpl100_build_settings.cmake)

# scaffold:platform-esp32
#===============================================================================
# ESP-IDF component
#===============================================================================
if(ESP_PLATFORM)
    idf_component_register(
        SRCS ${XMPL100_SOURCE_FILES}
        INCLUDE_DIRS ${XMPL100_PUBLIC_INCLUDE_DIRS}
        REQUIRES ${XMPL100_IDF_REQUIRES}
        KCONFIG ${CMAKE_CURRENT_LIST_DIR}/_config/Kconfig
    )
    if(XMPL100_SOURCE_FILES)
        target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
    else()
        target_compile_features(${COMPONENT_LIB} INTERFACE cxx_std_20)
    endif()
    return()
endif()
# /scaffold:platform-esp32

project(xmpl100_driver
    VERSION ${XMPL100_VERSION}
    DESCRIPTION "C++20 driver for the XMPL100, a fictional example SPI temperature sensor: chip ID, one-shot and continuous reads, CRC."
    LANGUAGES CXX)

include(GNUInstallDirs)

# ISO C++20 for every target this project builds (tests, fuzzers, examples), never the GNU dialect
# that CMake picks by default (handbook KB-CPP-001, KB-CPP20-001). CXX_EXTENSIONS does not travel
# through an INTERFACE target, so each folder that builds a target sets it as well.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The installed layout keeps the source tree's inc/ + src/ pair, because xmpl100.hpp includes
# "../src/xmpl100.ipp" relative to itself:
#   <prefix>/include/xmpl100/inc/*.hpp, xmpl100_version.h
#   <prefix>/include/xmpl100/src/xmpl100.ipp
set(XMPL100_INSTALL_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}/xmpl100/inc")

#===============================================================================
# Target
#===============================================================================
add_library(${XMPL100_TARGET_NAME} INTERFACE)
target_include_directories(${XMPL100_TARGET_NAME}
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/inc>
        $<BUILD_INTERFACE:${XMPL100_VERSION_HEADER_DIR}>
        $<INSTALL_INTERFACE:${XMPL100_INSTALL_INCLUDEDIR}>
)
target_compile_features(${XMPL100_TARGET_NAME} INTERFACE cxx_std_20)

# xmpl100::xmpl100 everywhere: the alias for add_subdirectory(), EXPORT_NAME for the installed package.
add_library(xmpl100::xmpl100 ALIAS ${XMPL100_TARGET_NAME})
set_target_properties(${XMPL100_TARGET_NAME} PROPERTIES EXPORT_NAME xmpl100)

option(XMPL100_ENABLE_WARNINGS "Add -Wall -Wextra -Wpedantic to every target that links xmpl100::xmpl100" OFF)
if(XMPL100_ENABLE_WARNINGS)
    target_compile_options(${XMPL100_TARGET_NAME} INTERFACE -Wall -Wextra -Wpedantic)
endif()

#===============================================================================
# Host tests, documentation snippets, install check (tests/)
#===============================================================================
option(XMPL100_BUILD_TESTS "Build the host tests in tests/" OFF)
option(XMPL100_BUILD_FUZZERS "Also build the libFuzzer targets in tests/fuzz/ (Clang only)" OFF)
if(XMPL100_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

#===============================================================================
# Install and package export
#===============================================================================
include(CMakePackageConfigHelpers)

install(TARGETS ${XMPL100_TARGET_NAME} EXPORT xmpl100Targets
    INCLUDES DESTINATION ${XMPL100_INSTALL_INCLUDEDIR})

install(DIRECTORY inc/ DESTINATION ${XMPL100_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")
install(FILES "${XMPL100_VERSION_HEADER}" DESTINATION ${XMPL100_INSTALL_INCLUDEDIR})
install(DIRECTORY src/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/xmpl100/src
    FILES_MATCHING PATTERN "*.ipp")

install(EXPORT xmpl100Targets
    FILE xmpl100Targets.cmake
    NAMESPACE xmpl100::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/xmpl100)

configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/cmake/xmpl100Config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/xmpl100Config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/xmpl100)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/xmpl100ConfigVersion.cmake"
    VERSION ${XMPL100_VERSION}
    COMPATIBILITY SameMajorVersion
    ARCH_INDEPENDENT)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/xmpl100Config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/xmpl100ConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/xmpl100)
