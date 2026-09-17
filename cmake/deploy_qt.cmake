# 将 Qt 运行时（DLL/插件/QML 模块）部署到可执行文件所在目录。
# 由根 CMakeLists 的 POST_BUILD 以 `cmake -P` 调用。
#
# 跳过条件：清单中每个 Qt 模块对应的 Qt6<模块>*.dll 都已存在（兼容 Debug 的 d 后缀）。
# 只要缺任意一个模块就重新执行 windeployqt —— 这样"旧构建目录里已有部分 Qt DLL"时
# 仍能补上后来新增的模块（WebSockets / Bluetooth / QuickControls2 等）。
#
# 入参：
#   WINDEPLOYQT     windeployqt 可执行文件
#   TARGET_FILE     目标可执行文件
#   TARGET_DIR      目标可执行文件所在目录
#   QML_DIR         项目 QML 源目录（供 windeployqt 扫描 QML 依赖）
#   DEPLOY_MODE     Debug 传 --debug，否则传 --release
#   DEPLOY_MODULES  需要检查的 Qt 模块名列表，用逗号分隔（如 Core,Gui,Quick,...）
#                   （不能用 ; 或 | ：自定义命令经 Windows cmd 执行时它们是命令/管道分隔符）

if(NOT EXISTS "${WINDEPLOYQT}")
	message(STATUS "windeployqt not found, skip Qt runtime deployment")
	return()
endif()

string(REPLACE "," ";" _deploy_modules "${DEPLOY_MODULES}")

set(_missing "")
foreach(_module IN LISTS _deploy_modules)
	file(GLOB _found "${TARGET_DIR}/Qt6${_module}*.dll")
	if(NOT _found)
		list(APPEND _missing "${_module}")
	endif()
endforeach()

if(NOT _missing)
	message(STATUS "Qt runtime complete in ${TARGET_DIR}, skip deployment")
	return()
endif()

message(STATUS "Deploying Qt runtime to ${TARGET_DIR} (missing: ${_missing}) ...")
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
	return()
endif()

set(_still_missing "")
foreach(_module IN LISTS _deploy_modules)
	file(GLOB _found "${TARGET_DIR}/Qt6${_module}*.dll")
	if(NOT _found)
		list(APPEND _still_missing "${_module}")
	endif()
endforeach()
if(_still_missing)
	message(WARNING "windeployqt 后仍缺少 Qt 模块: ${_still_missing}")
else()
	message(STATUS "Qt runtime deployed to ${TARGET_DIR}")
endif()
