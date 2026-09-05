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
)
target_include_directories(dn_core PUBLIC include)

# ---------------------------------------------------------------------------
# doctest: vendored single header (third_party/doctest.h).
# ---------------------------------------------------------------------------
add_library(doctest INTERFACE)
target_include_directories(doctest INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

# ---------------------------------------------------------------------------
# DupNames: Win32 GUI application.
# ---------------------------------------------------------------------------
add_executable(DupNames WIN32
  src/main.cpp
)
target_link_libraries(DupNames PRIVATE dn_core comctl32 gdiplus shell32)

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
  )
  target_link_libraries(dn_tests PRIVATE dn_core doctest)
  if(MSVC)
    # doctest.h specializes std::tuple; harmless under the VS 2026 STL.
    target_compile_options(dn_tests PRIVATE /wd5285)
  endif()
  add_test(NAME dn_tests COMMAND dn_tests)
endif()
