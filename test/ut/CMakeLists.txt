# -----------------------------------------------------------------------------
# ffrt ST
# -----------------------------------------------------------------------------
set(TARGET_NAME ffrt_ut)

file(GLOB_RECURSE UT_DIR_LIST
        "testcase/*.cpp"
        "../stub/*.cpp"
)

file(GLOB_RECURSE CGROUP _SRC "testcase/*_cgroup_qos.cpp")
list(REMOVE_ITEM UT_DIR_LIST ${CGROUP_SRC})

# third-party googletest
if (EXISTS ${ROOT_PATH}/third_party/googletest)
        add_subdirectory(${ROOT_PATH}/third_party/googletest ${FFRT_BUILD_PATH}/gtest)
        include_directories(${ROOT_PATH}/third_party/googletest/include/)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_GTEST")
else()
        add_subdirectory(${ROOT_PATH}/external/googletest ${FFRT_BUILD_PATH}/gtest)
        include_directories(${ROOT_PATH}/external/googletest/include/)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_GTEST")
endif()

add_definitions("-DFFRT_BBOX_ENABLE")
add_definitions("-DFFRT_WORKER_MONITOR")
add_definitions("-DQOS_WORKER_FRAME_RTG")

include_directories(${PROJECT_SOURCE_DIR}/test/stub/)

#mockcpp
set(MOCKCPP_SRC ${ROOT_PATH}/test/open_source/mockcpp-2.6)
add_subdirectory("${MOCKCPP_SRC}" mockcpp)
include_directories(${MOCKCPP_SRC}/include)
include_directories(${MOCKCPP_SRC}/3rdparty)

get_property(FFRT_SRC_LIST GLOBAL PROPERTY FFRT_SRC_LIST)
add_executable(${TARGET_NAME} ${FFRT_SRC_LIST} ${TEST_FUNC_DIR_LIST} ${UT_DIR_LIST} main.cpp)

target_link_libraries(${TARGET_NAME} -ldl)
target_link_libraries(${TARGET_NAME} securec)
target_link_libraries(${TARGET_NAME} -lpthread)
target_link_libraries(${TARGET_NAME} gtest gmock mockcpp)