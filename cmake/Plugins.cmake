function(dgl_add_plugin _target)
	cmake_parse_arguments(ARG "" "" "SOURCES;QT_LIBS;META_DIR" ${ARGN})
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
	# 主干名恒等于目标名，侧车目录名才能与之对应
	set_target_properties(${_target} PROPERTIES PREFIX "")

	# 展示元数据侧车与动态库同位复制（未指定 META_DIR 时不追加）
	set(_meta_commands)
	set(_meta_dirs)
	if(ARG_META_DIR)
		# 侧车目录在配置期解析为绝对路径，构建后复制与安装共用
		foreach(_meta IN LISTS ARG_META_DIR)
			get_filename_component(_meta_abs "${_meta}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
			list(APPEND _meta_dirs "${_meta_abs}")
		endforeach()
		list(APPEND _meta_commands
			COMMAND ${CMAKE_COMMAND} -E make_directory
				"$<TARGET_FILE_DIR:${PROJECT_NAME}>/module/${_target}"
		)
		foreach(_meta IN LISTS _meta_dirs)
			list(APPEND _meta_commands
				COMMAND ${CMAKE_COMMAND} -E copy_directory "${_meta}"
					"$<TARGET_FILE_DIR:${PROJECT_NAME}>/module/${_target}"
			)
		endforeach()
	endif()

	# 构建后复制到构建目录，供本地运行/调试
	add_custom_command(
		TARGET ${_target}
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E make_directory
			"$<TARGET_FILE_DIR:${PROJECT_NAME}>/module"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${_target}>"
			"$<TARGET_FILE_DIR:${PROJECT_NAME}>/module"
		${_meta_commands}
		COMMENT "Copying ${_target} to module directory"
	)

	# 只登记，不在这里 install
	set_property(GLOBAL APPEND PROPERTY DGLAB_PLUGIN_TARGETS ${_target})
	if(ARG_META_DIR)
		set_property(GLOBAL APPEND PROPERTY DGLAB_PLUGIN_METAS ${_target})
		set_property(GLOBAL PROPERTY DGLAB_PLUGIN_META_${_target} "${_meta_dirs}")
	endif()
endfunction()

# 统一安装所有已登记插件；必须在主程序 install(TARGETS ... BUNDLE DESTINATION .) 之后调用
function(dgl_install_plugins)
	get_property(_plugins GLOBAL PROPERTY DGLAB_PLUGIN_TARGETS)
	get_property(_meta_plugins GLOBAL PROPERTY DGLAB_PLUGIN_METAS)
	foreach(_t IN LISTS _plugins)
		if(WIN32)
			install(TARGETS ${_t} RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}/module")
			set(_module_dest "${CMAKE_INSTALL_BINDIR}/module")
		elseif(APPLE)
			# 必须放进 .app 内，dmg/zip 才会打包，codesign --deep 和公证才会覆盖
			install(
				TARGETS ${_t}
				LIBRARY DESTINATION "${PROJECT_NAME}.app/Contents/MacOS/module"
			)
			set(_module_dest "${PROJECT_NAME}.app/Contents/MacOS/module")
		else()
			install(TARGETS ${_t} LIBRARY DESTINATION "module")
			set(_module_dest "module")
		endif()
		# 侧车目录随动态库安装到同一目录下的 <目标名> 子目录
		if(_t IN_LIST _meta_plugins)
			get_property(_meta_dirs GLOBAL PROPERTY DGLAB_PLUGIN_META_${_t})
			foreach(_meta IN LISTS _meta_dirs)
				install(DIRECTORY "${_meta}/" DESTINATION "${_module_dest}/${_t}")
			endforeach()
		endif()
	endforeach()
endfunction()
