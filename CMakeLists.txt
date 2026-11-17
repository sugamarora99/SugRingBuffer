cmake_minimum_required(VERSION 3.16)
project(sug_ringbuffer VERSION 0.1.0 LANGUAGES CXX)

option(SUG_BUILD_TESTS "Build the test suite" ON)
option(SUG_BUILD_BENCHMARKS "Build benchmark programs" ON)
set(SUG_SANITIZE "" CACHE STRING "Sanitizer to build with: '' (none), 'thread', or 'address,undefined'")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

find_package(Threads REQUIRED)

# Warnings as errors apply to the LIBRARY (your code). Tests/benchmarks use
# relaxed warnings because GoogleTest headers are not ours to fix.
set(SUG_WARNINGS -Wall -Wextra -Wpedantic -Werror)
set(SUG_WARNINGS_RELAXED -Wall -Wextra -Wpedantic)

# Header-only templated ring buffers; no compiled sources.
add_library(sug_ringbuffer INTERFACE)
target_include_directories(sug_ringbuffer INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(sug_ringbuffer INTERFACE Threads::Threads)

if(SUG_SANITIZE)
  set(_san_flags -fsanitize=${SUG_SANITIZE} -fno-omit-frame-pointer)
  target_compile_options(sug_ringbuffer INTERFACE ${_san_flags})
  target_link_options(sug_ringbuffer INTERFACE ${_san_flags})
endif()

if(SUG_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(SUG_BUILD_BENCHMARKS)
  add_subdirectory(benchmarks)
endif()
