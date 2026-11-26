cmake_minimum_required(VERSION 3.20)
project(DupNames CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32)
  add_compile_definitions(UNICODE _UNICODE)
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(MSVC)
  add_compile_options(/W4 /permissive- /utf-8)
else()
  add_compile_options(-Wall -Wextra -Werror)
endif()

# ---------------------------------------------------------------------------
# dn_core: matching / scanning / persistence logic, shared by app and tests.
# ---------------------------------------------------------------------------
add_library(dn_core
  src/dn_version.cpp
  src/fold_table.cpp
  src/utf8.cpp
  src/normalize.cpp
  src/compare.cpp
  src/cluster.cpp
  src/blocking.cpp
  src/scanner.cpp
  src/match.cpp
  src/deletion.cpp
  src/ini.cpp
  src/cli.cpp
)
target_include_directories(dn_core PUBLIC include)

# ---------------------------------------------------------------------------
# doctest: vendored single header (third_party/doctest.h).
# ---------------------------------------------------------------------------
add_library(doctest INTERFACE)
target_include_directories(doctest INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

# ---------------------------------------------------------------------------
# Build date/time stamp: generated into the build tree at configure time and
# refreshed on every build (gen_build_info runs each build), so the About
# window always shows the date/time the build command was run.
# ---------------------------------------------------------------------------
set(DN_BUILD_INFO_DIR ${CMAKE_BINARY_DIR}/generated)
set(DN_BUILD_INFO_CPP ${DN_BUILD_INFO_DIR}/build_info.cpp)
set(DN_BUILD_INFO_HPP ${DN_BUILD_INFO_DIR}/build_info.hpp)
file(MAKE_DIRECTORY ${DN_BUILD_INFO_DIR})
execute_process(
  COMMAND ${CMAKE_COMMAND} -DBUILD_INFO_DIR=${DN_BUILD_INFO_DIR}
          -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateBuildInfo.cmake
)
add_custom_target(gen_build_info ALL
  COMMAND ${CMAKE_COMMAND} -DBUILD_INFO_DIR=${DN_BUILD_INFO_DIR}
          -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateBuildInfo.cmake
  COMMENT "Generating build date/time stamp"
  VERBATIM
)

# ---------------------------------------------------------------------------
# DupNames: Win32 GUI application.
# ---------------------------------------------------------------------------
add_executable(DupNames WIN32
  src/main.cpp
  src/gui/app.cpp
  src/gui/options.cpp
  src/gui/rename.cpp
  src/gui/about.cpp
  ${DN_BUILD_INFO_CPP}
  src/resources/DupNames.rc
)
target_include_directories(DupNames PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/resources
  ${DN_BUILD_INFO_DIR}
)
add_dependencies(DupNames gen_build_info)
target_link_libraries(DupNames PRIVATE dn_core comctl32 gdiplus shell32 ole32)
# MinGW/GCC: the app's entry point is wWinMain (wide), so select the wide-char
# CRT startup. MSVC picks this up automatically from UNICODE + the WIN32 subsystem.
if(NOT MSVC)
  target_link_options(DupNames PRIVATE -municode)
endif()

# ---------------------------------------------------------------------------
# Tests.
# ---------------------------------------------------------------------------
option(DN_TESTS "Build tests" ON)
if(DN_TESTS)
  include(CTest)
  add_executable(dn_tests
    tests/doctest_impl.cpp
    tests/test_smoke.cpp
    tests/test_utf8.cpp
    tests/test_normalize.cpp
    tests/test_compare.cpp
    tests/test_cluster.cpp
    tests/test_blocking.cpp
    tests/test_acceptance.cpp
    tests/test_scanner.cpp
    tests/test_match.cpp
    tests/test_deletion.cpp
    tests/test_ini.cpp
    tests/test_cli.cpp
    tests/test_determinism.cpp
    tests/test_perf.cpp
    tests/test_edge.cpp
  )
  target_link_libraries(dn_tests PRIVATE dn_core doctest)
  if(MSVC)
    # doctest.h specializes std::tuple; harmless under the VS 2026 STL.
    target_compile_options(dn_tests PRIVATE /wd5285)
  endif()
  add_test(NAME dn_tests COMMAND dn_tests)
endif()
