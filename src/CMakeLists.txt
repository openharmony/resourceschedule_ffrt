# -----------------------------------------------------------------------------
# ffrt code
# -----------------------------------------------------------------------------
set(FFRT_LOG_PLAT "linux")

file(GLOB_RECURSE FFRT_SRC_LIST
	"${FFRT_CODE_PATH}/core/*.cpp"
	"${FFRT_CODE_PATH}/eu/*.cpp"
	"${FFRT_CODE_PATH}/eu/*.c"
	"${FFRT_CODE_PATH}/internal_inc/*.cpp"
	"${FFRT_CODE_PATH}/queue/*.cpp"
	"${FFRT_CODE_PATH}/sched/*.cpp"
	"${FFRT_CODE_PATH}/sync/*.cpp"
	"${FFRT_CODE_PATH}/util/*.cpp"
	"${FFRT_CODE_PATH}/tm/*.cpp"
	"${FFRT_CODE_PATH}/dm/*.cpp"
	"${FFRT_CODE_PATH}/dfx/bbox/bbox.cpp"
	"${FFRT_CODE_PATH}/dfx/log/ffrt_log.cpp"
	"${FFRT_CODE_PATH}/dfx/log/${FFRT_LOG_PLAT}/log_base.cpp"
	"${FFRT_CODE_PATH}/dfx/watchdog/watchdog.cpp"
	"${FFRT_CODE_PATH}/dfx/watchdog/watchdog_util.cpp"
)

set_property(GLOBAL APPEND PROPERTY FFRT_SRC_LIST ${FFRT_SRC_LIST})

add_library(${PROJECT_NAME} SHARED ${FFRT_SRC_LIST})
target_link_libraries(${PROJECT_NAME} -ldl)
target_link_libraries(${PROJECT_NAME} securec)
target_link_libraries(${PROJECT_NAME} -lpthread)