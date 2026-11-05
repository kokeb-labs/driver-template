# =============================================================================
# XMPL100 driver: host tests
# =============================================================================
# The driver against a register model of the chip, with the firmware's flags
# (-fno-exceptions -fno-rtti) and warnings as errors. No hardware, no ESP-IDF:
#
#   cmake -S . -B build -DXMPL100_BUILD_TESTS=ON
#   cmake --build build
#   ctest --test-dir build --output-on-failure
# =============================================================================

set(XMPL100_TEST_FLAGS -fno-exceptions -fno-rtti -Wall -Wextra -Wpedantic -Wshadow -Wconversion
    -Wsign-conversion -Werror)

function(xmpl100_test_target name)
  target_link_libraries(${name} PRIVATE xmpl100::xmpl100)
  target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_compile_features(${name} PRIVATE cxx_std_20)
  target_compile_options(${name} PRIVATE ${XMPL100_TEST_FLAGS})
endfunction()

# --- Unit tests ---------------------------------------------------------------
file(GLOB XMPL100_UNIT_TESTS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp)
list(FILTER XMPL100_UNIT_TESTS EXCLUDE REGEX "test_(main|version|kconfig)\\.cpp$")
add_executable(xmpl100_host_tests test_main.cpp ${XMPL100_UNIT_TESTS})
xmpl100_test_target(xmpl100_host_tests)
add_test(NAME xmpl100_host_tests COMMAND xmpl100_host_tests)

# GCC's flow warnings (-Warray-bounds, -Wstringop-*) only fire with optimisation, and the ESP-IDF Release
# builds promote them to errors: compile the tests at -O2 as well, whatever CMAKE_BUILD_TYPE is.
add_library(xmpl100_optimised_build OBJECT ${XMPL100_UNIT_TESTS})
xmpl100_test_target(xmpl100_optimised_build)
target_compile_options(xmpl100_optimised_build PRIVATE -O2)

# --- Kconfig layer: without and with CONFIG_ values ---------------------------
add_executable(xmpl100_kconfig_defaults test_main.cpp test_kconfig.cpp)
xmpl100_test_target(xmpl100_kconfig_defaults)
add_test(NAME xmpl100_kconfig_defaults COMMAND xmpl100_kconfig_defaults)

add_executable(xmpl100_kconfig_set test_main.cpp test_kconfig.cpp)
xmpl100_test_target(xmpl100_kconfig_set)
# scaffold:kconfig-test-defines
target_compile_definitions(xmpl100_kconfig_set PRIVATE KCONFIG_SET=1 CONFIG_XMPL100=1
  CONFIG_XMPL100_ODR_8HZ=1 CONFIG_XMPL100_AVERAGING_SAMPLES=4 CONFIG_XMPL100_CRC_ENABLE=1
  CONFIG_XMPL100_CONVERSION_TIMEOUT_MS=250)
# /scaffold:kconfig-test-defines
add_test(NAME xmpl100_kconfig_set COMMAND xmpl100_kconfig_set)

# --- Version: the getters, and every copy of the version string ---------------
function(xmpl100_version_test name include_dir major minor patch)
  add_executable(${name} test_version.cpp)
  if(include_dir)
    target_include_directories(${name} BEFORE PRIVATE ${include_dir})
  endif()
  xmpl100_test_target(${name})
  target_compile_definitions(${name} PRIVATE EXPECTED_MAJOR=${major} EXPECTED_MINOR=${minor}
    EXPECTED_PATCH=${patch} EXPECTED_STRING="${major}.${minor}.${patch}")
  add_test(NAME ${name} COMMAND ${name})
endfunction()

xmpl100_version_test(xmpl100_version "" ${PROJECT_VERSION_MAJOR} ${PROJECT_VERSION_MINOR}
  ${PROJECT_VERSION_PATCH})

# The same template with another version, found ahead of the real header.
function(xmpl100_version_probe_header)
  set(XMPL100_VERSION_MAJOR 9)
  set(XMPL100_VERSION_MINOR 8)
  set(XMPL100_VERSION_PATCH 7)
  set(XMPL100_VERSION 9.8.7)
  configure_file(${XMPL100_VERSION_TEMPLATE} ${CMAKE_CURRENT_BINARY_DIR}/version_probe/xmpl100_version.h @ONLY)
endfunction()
xmpl100_version_probe_header()
xmpl100_version_test(xmpl100_version_probe ${CMAKE_CURRENT_BINARY_DIR}/version_probe 9 8 7)

# Files that carry their own copy of the version must match cmake/xmpl100_build_settings.cmake.
foreach(copy "_config/Doxyfile|PROJECT_NUMBER")
  string(REPLACE "|" ";" parts "${copy}")
  list(GET parts 0 file)
  list(GET parts 1 key)
  string(MAKE_C_IDENTIFIER "${file}" id)
  add_test(NAME xmpl100_version_copy_${id}
    COMMAND ${CMAKE_COMMAND} -DFILE=${PROJECT_SOURCE_DIR}/${file} -DKEY=${key} -DVERSION=${PROJECT_VERSION}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/check_version_copy.cmake)
endforeach()

# --- Documentation snippets: compiled, never linked or run --------------------
# Every C++ block on the documentation site is included from tests/docs/ (pymdownx.snippets), so a page
# cannot show code that does not compile against the current headers.
file(GLOB XMPL100_DOC_SNIPPETS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/docs/*.cpp)
add_library(xmpl100_doc_snippets OBJECT ${XMPL100_DOC_SNIPPETS})
xmpl100_test_target(xmpl100_doc_snippets)
target_include_directories(xmpl100_doc_snippets PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/docs)

# --- Host example (examples/host/): built with the test flags and run ------------
add_subdirectory(${PROJECT_SOURCE_DIR}/examples/host ${CMAKE_CURRENT_BINARY_DIR}/examples-host)
xmpl100_test_target(xmpl100_host_example)
add_test(NAME xmpl100_host_example COMMAND xmpl100_host_example)

# --- Install rules and the find_package() consumer ----------------------------
add_test(NAME xmpl100_install_consumer
  COMMAND ${CMAKE_COMMAND}
    -DBUILD_DIR=${PROJECT_BINARY_DIR}
    -DSOURCE_DIR=${PROJECT_SOURCE_DIR}
    -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/install-check
    -DCXX_COMPILER=${CMAKE_CXX_COMPILER}
    "-DGENERATOR=${CMAKE_GENERATOR}"
    -P ${CMAKE_CURRENT_SOURCE_DIR}/check_install.cmake)

# --- Doxygen: every public symbol in inc/ documented (when doxygen is installed) ---
find_program(XMPL100_DOXYGEN doxygen)
if(XMPL100_DOXYGEN)
  add_test(NAME xmpl100_doxygen
    COMMAND ${CMAKE_COMMAND} -DDOXYGEN=${XMPL100_DOXYGEN} -DSOURCE_DIR=${PROJECT_SOURCE_DIR}
            -DOUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/doxygen -P ${CMAKE_CURRENT_SOURCE_DIR}/check_doxygen.cmake)
endif()

# --- Compile-time contract: these must fail to build, for the reason in their EXPECT: line ---
file(GLOB XMPL100_COMPILE_FAIL CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/compile_fail/*.cpp)
foreach(src ${XMPL100_COMPILE_FAIL})
  get_filename_component(stem ${src} NAME_WE)
  file(STRINGS ${src} expect REGEX "EXPECT: ")
  string(REGEX REPLACE ".*EXPECT: " "" expect "${expect}")
  add_executable(xmpl100_compile_fail_${stem} EXCLUDE_FROM_ALL ${src})
  xmpl100_test_target(xmpl100_compile_fail_${stem})
  add_test(NAME xmpl100_compile_fail_${stem}
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target xmpl100_compile_fail_${stem})
  # Pass only when the compiler rejects the code with the expected diagnostic, not another error.
  set_tests_properties(xmpl100_compile_fail_${stem} PROPERTIES PASS_REGULAR_EXPRESSION "${expect}")
endforeach()
