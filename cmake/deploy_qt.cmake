# 将 Qt 运行时（DLL/插件/QML 模块）部署到可执行文件所在目录。
# 由根 CMakeLists 的 POST_BUILD 以 `cmake -P` 调用；已部署则跳过，避免每次构建都重跑 windeployqt。
#
# 入参：
#   WINDEPLOYQT  windeployqt 可执行文件
#   TARGET_FILE  目标可执行文件
#   TARGET_DIR   目标可执行文件所在目录
#   QML_DIR      项目 QML 源目录（供 windeployqt 扫描 QML 依赖）
#   DEPLOY_MODE  Debug 时传 --debug，否则传 --release

if(NOT EXISTS "${WINDEPLOYQT}")
	message(STATUS "windeployqt not found, skip Qt runtime deployment")
	return()
endif()

file(GLOB _qt_core_dlls "${TARGET_DIR}/Qt6Core*.dll")
if(_qt_core_dlls)
	message(STATUS "Qt runtime already present in ${TARGET_DIR}, skip deployment")
	return()
endif()

message(STATUS "Deploying Qt runtime to ${TARGET_DIR} ...")
set(_args ${DEPLOY_MODE} --no-translations --no-system-d3d-compiler --no-compiler-runtime)
if(EXISTS "${QML_DIR}")
	list(APPEND _args --qmldir "${QML_DIR}")
endif()
list(APPEND _args "${TARGET_FILE}")

execute_process(
	COMMAND "${WINDEPLOYQT}" ${_args}
	RESULT_VARIABLE _result
	OUTPUT_VARIABLE _output
	ERROR_VARIABLE _error
)
if(NOT _result EQUAL 0)
	message(WARNING "windeployqt failed (${_result}): ${_error}")
else()
	message(STATUS "Qt runtime deployed to ${TARGET_DIR}")
endif()
