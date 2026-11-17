include(FetchContent)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.14.0
)

# Silence "Downloading..." noise and avoid installing gtest alongside our lib.
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(sug_ringbuffer_tests
  test_ring_buffer.cpp
)

target_include_directories(sug_ringbuffer_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(sug_ringbuffer_tests PRIVATE ${SUG_WARNINGS_RELAXED})
target_link_libraries(sug_ringbuffer_tests PRIVATE sug_ringbuffer gtest_main)

add_test(NAME sug_ringbuffer_tests COMMAND sug_ringbuffer_tests)
