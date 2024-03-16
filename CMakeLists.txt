# -------------------------------------------------------------------
# ffrt CMake
#
# To enable thread sanitizer:
# -DCMAKE_CXX_FLAGS="-fsanitize=thread -g"
#
# To enable address and leak sanitizers
# -DCMAKE_CXX_FLAGS="-fsanitize=address  -fsanitize=leak -g"
# -------------------------------------------------------------------

# CMake version
cmake_minimum_required (VERSION 3.10)

# build options
option(FFRT_BBOX_ENABLE "Enables ffrt black box" ON)
option(FFRT_EXAMPLE "Enables ffrt examples compile" ON)
option(FFRT_BENCHMARKS "Enables ffrt Benchmarks" OFF)
option(FFRT_TEST_ENABLE "Enables ffrt test" OFF)
option(FFRT_CLANG_COMPILE "use clang/clang++ for compiling" OFF)
option(FFRT_SANITIZE "enable address or thread sanitizer" OFF)
option(FFRT_IO_TASK_SCHEDULER "enalbe io task scheduler" OFF)

# set compiler clang or gcc, must before project(ffrt)
if(FFRT_CLANG_COMPILE STREQUAL ON)
  set(CMAKE_C_COMPILER clang)
  set(CMAKE_CXX_COMPILER clang++)
else()
  set(CMAKE_C_COMPILER gcc)
  set(CMAKE_CXX_COMPILER g++)
endif()

# Project name
project(ffrt)

#cmake build type settings,release or debug
if(CMAKE_BUILD_TYPE STREQUAL Release)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DFFRT_RELEASE -DFFRT_LOG_LEVEL=0")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DFFRT_LOG_LEVEL=3")
endif()

# project-specific variables
set(FFRT_CODE_PATH ${PROJECT_SOURCE_DIR}/src)
set(FFRT_INNER_API_PATH ${PROJECT_SOURCE_DIR}/interfaces/inner_api)
set(FFRT_API_PATH ${PROJECT_SOURCE_DIR}/interfaces/kits)
set(FFRT_BUILD_PATH ${PROJECT_SOURCE_DIR}/build)
set(ROOT_PATH ${PROJECT_SOURCE_DIR}/../)
set(SECUREC_PATH ${ROOT_PATH}/third_party/bounds_checking_function)
set(TRACE_PATH ${FFRT_CODE_PATH}/dfx/trace)
set(QOS_PATH ${FFRT_CODE_PATH}/sched/)
set(securec_includes ${SECUREC_PATH}/include)
aux_source_directory(${SECUREC_PATH}/src securec_srcs)
add_library(securec SHARED ${securec_srcs})
target_include_directories(securec PRIVATE ${securec_includes})
include_directories(${securec_includes})

#compiler flags
if(FFRT_SANITIZE STREQUAL address)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fsanitize-recover=address,all -fno-omit-frame-pointer")
elseif(FFRT_SANITIZE STREQUAL thread)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread -fno-omit-frame-pointer")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
if(CMAKE_BUILD_TYPE STREQUAL Release)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -g -O2 -fPIC")
else()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -g -O0 -fPIC")
endif()

# add the binary tree to the search path for include files
include_directories(${FFRT_API_PATH})
include_directories(${FFRT_INNER_API_PATH})
include_directories(${FFRT_CODE_PATH})
include_directories(${TRACE_PATH})
include_directories(${QOS_PATH})

message(STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE})
message(STATUS "FFRT_BBOX_ENABLE: " ${FFRT_BBOX_ENABLE})
message(STATUS "FFRT_EXAMPLE: " ${FFRT_EXAMPLE})
message(STATUS "FFRT_BENCHMARKS: " ${FFRT_BENCHMARKS})
message(STATUS "FFRT_TEST_ENABLE: " ${FFRT_TEST_ENABLE})
message(STATUS "FFRT_CLANG_COMPILE: " ${FFRT_CLANG_COMPILE})
message(STATUS "FFRT_SANITIZE: " ${FFRT_SANITIZE})

add_definitions("-DFFRT_IO_TASK_SCHEDULER")
add_definitions("-DFFRT_PTHREAD_ENABLE")

# ffrt qos
add_definitions(-DUSE_OHOS_QOS)

# ffrt BBOX
if(FFRT_BBOX_ENABLE STREQUAL ON)
  add_definitions("-DFFRT_BBOX_ENABLE")
endif()

#ffrt io task scheduler
if(FFRT_IO_TASK_SCHEDULER STREQUAL ON)
  add_definitions(-DFFRT_IO_TASK_SCHEDULER)
endif()

# libffrt.so
add_subdirectory(src)
LINK_DIRECTORIES(${FFRT_BUILD_PATH}/src)

# ffrt Examples
if(FFRT_EXAMPLE STREQUAL ON)
  add_subdirectory(examples)
endif()

# ffrt Benchmarks
if(FFRT_BENCHMARKS STREQUAL ON)
  add_subdirectory(benchmarks)
endif()
