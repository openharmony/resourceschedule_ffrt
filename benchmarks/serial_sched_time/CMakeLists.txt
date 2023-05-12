# -----------------------------------------------------------------------------
# Benchmarks schedule time test
# -----------------------------------------------------------------------------

add_executable(serial_sched_time_test serial_sched_time.cpp)
target_link_libraries(serial_sched_time_test ${FFRT_LD_FLAGS})