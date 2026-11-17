# The benchmark is Linux-only (it is meant to be profiled with `perf`).
# On other platforms this subdirectory builds nothing.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_executable(sug_ringbuffer_bench ring_buffer_bench.cpp)
  target_compile_options(sug_ringbuffer_bench PRIVATE ${SUG_WARNINGS_RELAXED})
  target_link_libraries(sug_ringbuffer_bench PRIVATE sug_ringbuffer)
endif()
