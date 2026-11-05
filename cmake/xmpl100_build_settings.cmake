#===============================================================================
# XMPL100 driver: build settings
#===============================================================================
# Read by the root CMakeLists.txt in both of its modes (plain CMake and the
# ESP-IDF component): target name, version, include directories, sources and
# ESP-IDF requirements. This file is the single source
# of the version; tests/CMakeLists.txt checks every other copy against it.
#===============================================================================

include_guard(GLOBAL)

set(XMPL100_TARGET_NAME "xmpl100")

#===============================================================================
# Version (single source of truth)
#===============================================================================
set(XMPL100_VERSION_MAJOR 0)
set(XMPL100_VERSION_MINOR 1)
set(XMPL100_VERSION_PATCH 0)
set(XMPL100_VERSION "${XMPL100_VERSION_MAJOR}.${XMPL100_VERSION_MINOR}.${XMPL100_VERSION_PATCH}")

#===============================================================================
# Generated version header (build tree, never the source tree)
#===============================================================================
set(XMPL100_VERSION_TEMPLATE "${CMAKE_CURRENT_LIST_DIR}/../inc/xmpl100_version.h.in")
set(XMPL100_VERSION_HEADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/xmpl100_generated")
set(XMPL100_VERSION_HEADER "${XMPL100_VERSION_HEADER_DIR}/xmpl100_version.h")

# The directory must exist before ESP-IDF's idf_component_register() checks the include paths.
file(MAKE_DIRECTORY "${XMPL100_VERSION_HEADER_DIR}")
configure_file("${XMPL100_VERSION_TEMPLATE}" "${XMPL100_VERSION_HEADER}" @ONLY)

#===============================================================================
# Public include directories, sources, ESP-IDF requirements
#===============================================================================
set(XMPL100_PUBLIC_INCLUDE_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../inc"
    "${XMPL100_VERSION_HEADER_DIR}"
)

# Header-only: the template implementation is src/xmpl100.ipp, included by inc/xmpl100.hpp.
set(XMPL100_SOURCE_FILES)

set(XMPL100_IDF_REQUIRES)
