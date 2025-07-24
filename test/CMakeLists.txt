include(FetchContent)

# enable testing
enable_testing()

set(FETCHCONTENT_QUIET FALSE)

# fetch googletest
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://gitee.com/openharmony/third_party_googletest.git
  GIT_TAG f92157de8552e7b4e01d059b2d9a6999c635ab2e
  GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(googletest)

# fetch all cpp files
file(GLOB_RECURSE UT_TESTS "ut/*.cpp")

# fetch all util files used by the tests
file(GLOB_RECURSE UTILS "testfunc/*.cpp")
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/testfunc)

# Executable/target name
set(TEST_NAME ffrt_ut)

add_executable(${TEST_NAME} ${UT_TESTS} ${UTILS})

set(COMPILE_DEFS FFRT_GITEE USE_GTEST WITH_NO_MOCKER)

if(NOT EXISTS "/proc/self/sched_rtg_ctrl")
  set(COMPILE_DEFS ${COMPILE_DEFS} SCHED_RGT_CTRL_DNE)
endif()

if(NOT EXISTS "/proc/thread-self/sched_qos_ctrl")
  set(COMPILE_DEFS ${COMPILE_DEFS} SCHED_QOS_CTRL_DNE)
endif()

if(NOT EXISTS "/dev/auth_ctrl")
  set(COMPILE_DEFS ${COMPILE_DEFS} AUTH_CTRL_DNE)
endif()

if(NOT EXISTS "/lib/libeventhandler.z.so")
  set(COMPILE_DEFS ${COMPILE_DEFS} LIB_EVENT_HANDLER_DNE)
endif()

# add compile definitions/macros
add_compile_definitions(${TEST_NAME} ${COMPILE_DEFS})

# link with gtest
target_link_libraries(
  ${TEST_NAME}
  GTest::gtest_main
  ffrt securec
  gmock
)

# add ctests using google-test
include(GoogleTest)
gtest_discover_tests(${TEST_NAME})
