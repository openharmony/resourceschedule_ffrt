# -----------------------------------------------------------------------------
# Benchmarks speedup test
# -----------------------------------------------------------------------------

add_executable(speedup_test speedup.cpp)
target_link_libraries(speedup_test ${FFRT_LD_FLAGS})