function(dgl_add_plugin _target)
	cmake_parse_arguments(ARG "" "" "SOURCES;QT_LIBS" ${ARGN})
	if(NOT ARG_SOURCES)
		message(FATAL_ERROR "dgl_add_plugin(${_target}): 缺少 SOURCES")
	endif()

	add_library(${_target} SHARED
		${ARG_SOURCES}
		# ModuleValue 为纯 C++ 模型，编入插件以便插件侧构造数值
		src/module/ModuleValue.cpp
	)
	target_compile_definitions(${_target} PRIVATE PLUGIN_BUILD)
	target_include_directories(${_target}
		PRIVATE
			${CMAKE_CURRENT_SOURCE_DIR}/include/plugin
			${CMAKE_CURRENT_SOURCE_DIR}/include/module
	)
	if(ARG_QT_LIBS)
		target_link_libraries(${_target} PRIVATE ${ARG_QT_LIBS})
	endif()
	if(WIN32)
		set_target_properties(${_target} PROPERTIES PREFIX "")
	endif()

	# 构建后复制到构建目录，供本地运行/调试
	add_custom_command(
		TARGET ${_target}
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E make_directory
			"$<TARGET_FILE_DIR:${PROJECT_NAME}>/module"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${_target}>"
			"$<TARGET_FILE_DIR:${PROJECT_NAME}>/module"
		COMMENT "Copying ${_target} to module directory"
	)

	# 只登记，不在这里 install
	set_property(GLOBAL APPEND PROPERTY DGLAB_PLUGIN_TARGETS ${_target})
endfunction()

# 统一安装所有已登记插件；必须在主程序 install(TARGETS ... BUNDLE DESTINATION .) 之后调用
function(dgl_install_plugins)
	get_property(_plugins GLOBAL PROPERTY DGLAB_PLUGIN_TARGETS)
	foreach(_t IN LISTS _plugins)
		if(WIN32)
			install(TARGETS ${_t} RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}/module")
		elseif(APPLE)
			# 必须放进 .app 内，dmg/zip 才会打包，codesign --deep 和公证才会覆盖
			install(
				TARGETS ${_t}
				LIBRARY DESTINATION "${PROJECT_NAME}.app/Contents/MacOS/module"
			)
		else()
			install(TARGETS ${_t} LIBRARY DESTINATION "module")
		endif()
	endforeach()
endfunction()
