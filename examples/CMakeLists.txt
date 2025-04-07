# -----------------------------------------------------------------------------
# ffrt EXAMPLES
# -----------------------------------------------------------------------------

set(FFRT_EXAMPLE_PATH ${PROJECT_SOURCE_DIR}/examples)

add_executable(ffrt_submit ${FFRT_EXAMPLE_PATH}/submit.cpp)

target_link_libraries(ffrt_submit PRIVATE ffrt)
