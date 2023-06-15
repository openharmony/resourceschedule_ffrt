# -----------------------------------------------------------------------------
# Benchmarks
# -----------------------------------------------------------------------------
set(FFRT_BENCHMARK_PATH ${PROJECT_SOURCE_DIR}/benchmarks)
include_directories(${FFRT_BENCHMARK_PATH})

option(BENCHMARKS_BASE "Enables Benchmarks Base" ON)
option(BENCHMARKS_FORK_JOIN "Enables Benchmarks Fork Join" ON)
option(BENCHMARKS_FIB "Enables Benchmarks FIB" ON)
option(BENCHMARKS_FACE_STORY "Enables Benchmarks Face Story" ON)
option(BENCHMARKS_SPEEDUP "Enables Speedup test" ON)
option(BENCHMARKS_SERIAL_SCHED_TIME "Enables completely serial schedule time test" ON)

message(STATUS "BENCHMARKS_BASE: " ${BENCHMARKS_BASE})
message(STATUS "BENCHMARKS_FORK_JOIN: " ${BENCHMARKS_FORK_JOIN})
message(STATUS "BENCHMARKS_FIB: " ${BENCHMARKS_FIB})
message(STATUS "BENCHMARKS_FACE_STORY: " ${BENCHMARKS_FACE_STORY})
message(STATUS "BENCHMARKS_SPEEDUP: " ${BENCHMARKS_SPEEDUP})
message(STATUS "BENCHMARKS_SERIAL_SCHED_TIME: " ${BENCHMARKS_SERIAL_SCHED_TIME})

LINK_DIRECTORIES(${FFRT_BUILD_PATH})

set(FFRT_LD_FLAGS "-lffrt")

if (BENCHMARKS_BASE STREQUAL ON)
    add_executable(base ${FFRT_BENCHMARK_PATH}/base/base.cpp)
    target_link_libraries(base ${FFRT_LD_FLAGS})
endif()

if (BENCHMARKS_FIB STREQUAL ON)
    add_executable(fib ${FFRT_BENCHMARK_PATH}/fib/fib.cpp)
    target_link_libraries(fib ${FFRT_LD_FLAGS})
endif()

if (BENCHMARKS_FORK_JOIN STREQUAL ON)
    add_executable(fork_join ${FFRT_BENCHMARK_PATH}/fork_join/fork_join.cpp)
    target_link_libraries(fork_join ${FFRT_LD_FLAGS})
endif()

if (BENCHMARKS_FACE_STORY STREQUAL ON)
    add_executable(face_story ${FFRT_BENCHMARK_PATH}/face_story/face_story.cpp)
    target_link_libraries(face_story ${FFRT_LD_FLAGS})
endif()

# speedup test
if (BENCHMARKS_SPEEDUP STREQUAL ON)
    add_subdirectory(speedup)
endif()

# serial schedule time test
if (BENCHMARKS_SERIAL_SCHED_TIME STREQUAL ON)
    add_subdirectory(serial_sched_time)
endif()