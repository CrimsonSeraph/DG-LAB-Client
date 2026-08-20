# 要求 CMake 最低版本 3.16
cmake_minimum_required(VERSION 3.16)
project(DG-LAB-Client LANGUAGES CXX)

# 引入 Qt 相关设置
include(qt.cmake)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Qt 相关设置（自动处理 uic/moc/rcc）
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# -------------------- 第三方库自动获取（FetchContent）--------------------
include(FetchContent)

# -------------------- 手动获取 nlohmann/json 单头文件 --------------------
set(NLOHMANN_JSON_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/include")
set(NLOHMANN_JSON_HEADER "${NLOHMANN_JSON_INCLUDE_DIR}/nlohmann/json.hpp")

# 若文件不存在，或上次下载失败留下了 0 字节的空文件，则视为缺失并重新下载
set(_NLOHMANN_JSON_NEED_DOWNLOAD TRUE)
if(EXISTS "${NLOHMANN_JSON_HEADER}")
    file(SIZE "${NLOHMANN_JSON_HEADER}" _NLOHMANN_JSON_SIZE)
    if(_NLOHMANN_JSON_SIZE GREATER 0)
        set(_NLOHMANN_JSON_NEED_DOWNLOAD FALSE)
        message(STATUS "json.hpp already exists, skipping download")
    else()
        message(STATUS "json.hpp exists but is empty (previous download failed), re-downloading...")
    endif()
endif()

if(_NLOHMANN_JSON_NEED_DOWNLOAD)
    message(STATUS "Downloading json.hpp from GitHub...")
    file(MAKE_DIRECTORY "${NLOHMANN_JSON_INCLUDE_DIR}/nlohmann")
    file(DOWNLOAD
        "https://github.com/nlohmann/json/releases/download/v3.12.0/json.hpp"
        "${NLOHMANN_JSON_HEADER}"
        STATUS DOWNLOAD_STATUS
        SHOW_PROGRESS
    )
    list(GET DOWNLOAD_STATUS 0 STATUS_CODE)
    if(STATUS_CODE EQUAL 0)
        message(STATUS "Downloaded json.hpp successfully")
    else()
        list(GET DOWNLOAD_STATUS 1 ERROR_MSG)
        message(FATAL_ERROR "Failed to download json.hpp: ${ERROR_MSG}")
    endif()
endif()

add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE "${NLOHMANN_JSON_INCLUDE_DIR}")

# -------------------- 查找 Qt --------------------
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets Network Qml)

if (QT_VERSION_MAJOR EQUAL 6)
    qt_standard_project_setup()
endif()

# -------------------- 项目源文件列表 --------------------
set(PROJECT_SOURCES
    # 应用外壳
    main.cpp
    include/DGLABClient.ui
    include/DGLABClient.h
    src/DGLABClient.cpp
    include/DGLABClient_utils.hpp
    include/ComboBoxDelegate.h
    src/ComboBoxDelegate.cpp
    include/FormulaBuilderDialog.h
    src/FormulaBuilderDialog.cpp
    include/ValueModeDelegate.h
    src/ValueModeDelegate.cpp
    include/EditableLabel.h
    src/EditableLabel.cpp
    include/IpSelector.h
    src/IpSelector.cpp

    # 资源文件
    DGLABClient.qrc

    # Python 调用接口
    include/PythonSubprocessManager.h
    src/PythonSubprocessManager.cpp

    # 配置管理器
    include/ConfigManager.h
    src/ConfigManager.cpp
    # 配置管理工厂
    include/MultiConfigManager.h
    src/MultiConfigManager.cpp
    include/MultiConfigManager_impl.hpp
    # 配置接口管理
    include/AppConfig.h
    src/AppConfig.cpp
    include/AppConfig_impl.hpp
    # 配置样式
    include/ConfigStructs.h
    src/ConfigStructs.cpp
    include/DefaultConfigs.h
    src/DefaultConfigs.cpp

    # 规则管理器
    include/Rule.h
    src/Rule.cpp
    include/RuleManager.h
    src/RuleManager.cpp
    include/RuleManager_impl.hpp

    # 波形绘制
    include/SampledWaveformWidget.h
    src/SampledWaveformWidget.cpp

    # 主题选择器
    include/ThemeSelectorDialog.h
    src/ThemeSelectorDialog.cpp

    # Debug 日志
    include/Console.h
    src/Console.cpp
    include/DebugLog.h
    src/DebugLog.cpp
    include/DebugLog_utils.hpp

    # 配置文件
    config/main.json
    config/system.json
    config/user.json
    config/rules/rules.json
)

# -------------------- 创建可执行文件 --------------------
if (QT_VERSION_MAJOR EQUAL 6)
    qt_add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})
else()
    add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})
endif()

if(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE TRUE)
endif()

if(WIN32)
    set_target_properties(${PROJECT_NAME}
        PROPERTIES WIN32_EXECUTABLE TRUE
    )
endif()

# 复制配置文件、Python脚本、资源、样式表（构建后）
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/config"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/config"
    COMMENT "Copying configuration files to output directory"
)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/python"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/python"
    COMMENT "Copying Python scripts to output directory"
)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
        COMMENT "Copying assets to output directory"
    )
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/qcss/")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/qcss"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/qcss"
        COMMENT "Copying qcss files to output directory"
    )
endif()

# -------------------- 查找 Python --------------------
find_package(Python 3.9 COMPONENTS Interpreter Development REQUIRED)

# ----- macOS：移除 Qt 链接参数中对已废弃 AGL.framework 的引用 -----
# Qt 6.5.x 及更早版本的官方 macOS 二进制在其 .prl 文件中带有 "-framework AGL"，
# 而新版 macOS（15+/26+）SDK 已移除 AGL.framework，链接时会报：
#   ld: framework 'AGL' not found
# Qt 在安装时会把 .prl 中的框架依赖写入导入目标的多个属性，常见存放位置：
#   - INTERFACE_LINK_LIBRARIES
#   - INTERFACE_LINK_OPTIONS
#   - IMPORTED_LINK_DEPENDENT_LIBRARIES / IMPORTED_LINK_DEPENDENT_LIBRARIES_<CONFIG>
#   - IMPORTED_LINK_INTERFACE_LIBRARIES / IMPORTED_LINK_INTERFACE_LIBRARIES_<CONFIG>
# 这里对项目用到的所有 Qt 导入目标的上述属性统一剥离 AGL 引用，兼容
# "-framework AGL" 单条目、"-framework" 与 "AGL" 分列两个条目、"-lAGL"、
# "-Wl,-framework,AGL"、AGL.framework / AGL.dylib 路径等多种写法。
# 注意：必须使用带版本号的 Qt${QT_VERSION_MAJOR}::Xxx 目标；
# 无版本号目标（Qt::Xxx）只是 ALIAS，不允许被 set_target_properties 修改。

# 从列表变量中移除 AGL 相关条目（结果写回父作用域同名变量）
function(dgl_strip_agl_list _var_name)
    set(_entries ${${_var_name}})
    set(_clean "")
    set(_changed FALSE)
    list(LENGTH _entries _len)
    set(_i 0)
    while(_i LESS _len)
        list(GET _entries ${_i} _cur)
        if(_cur STREQUAL "-framework AGL" OR _cur STREQUAL "AGL" OR _cur STREQUAL "-lAGL")
            # 单条目形式，直接丢弃
            set(_changed TRUE)
            math(EXPR _i "${_i} + 1")
            continue()
        endif()
        if(_cur STREQUAL "-framework")
            # 检查下一项是否是要被移除的 AGL，若是则成对丢弃
            math(EXPR _j "${_i} + 1")
            set(_next "")
            if(_j LESS _len)
                list(GET _entries ${_j} _next)
            endif()
            if(_next STREQUAL "AGL")
                set(_changed TRUE)
                math(EXPR _i "${_i} + 2")
                continue()
            endif()
        endif()
        if(_cur MATCHES "AGL\.(framework|dylib)$" OR _cur MATCHES "-Wl,.*AGL" OR _cur MATCHES "-framework,AGL")
            # 路径形式 / 链接器直传形式
            set(_changed TRUE)
        else()
            list(APPEND _clean "${_cur}")
        endif()
        math(EXPR _i "${_i} + 1")
    endwhile()
    set(${_var_name} "${_clean}" PARENT_SCOPE)
    if(_changed)
        set(_DGL_AGL_STRIPPED TRUE PARENT_SCOPE)
    endif()
endfunction()

# 剥离单个 Qt 导入目标上所有可能携带 AGL 引用的属性
function(dgl_strip_agl_target _target)
    if(NOT TARGET ${_target})
        return()
    endif()
    set(_DGL_AGL_STRIPPED FALSE)

    # 现代链接属性
    foreach(_prop IN ITEMS INTERFACE_LINK_LIBRARIES INTERFACE_LINK_OPTIONS)
        get_target_property(_val ${_target} ${_prop})
        if(_val)
            dgl_strip_agl_list(_val)
            set_target_properties(${_target} PROPERTIES ${_prop} "${_val}")
        endif()
    endforeach()

    # 旧式 IMPORTED_LINK_* 属性（含按配置区分的变体）
    foreach(_prop IN ITEMS IMPORTED_LINK_DEPENDENT_LIBRARIES IMPORTED_LINK_INTERFACE_LIBRARIES)
        get_target_property(_val ${_target} ${_prop})
        if(NOT _val STREQUAL "_val-NOTFOUND")
            dgl_strip_agl_list(_val)
            set_target_properties(${_target} PROPERTIES ${_prop} "${_val}")
        endif()
        foreach(_cfg IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
            get_target_property(_val ${_target} ${_prop}_${_cfg})
            if(NOT _val STREQUAL "_val-NOTFOUND")
                dgl_strip_agl_list(_val)
                set_target_properties(${_target} PROPERTIES ${_prop}_${_cfg} "${_val}")
            endif()
        endforeach()
    endforeach()

    if(_DGL_AGL_STRIPPED)
        message(STATUS "Removed deprecated AGL framework reference from ${_target}")
    endif()
endfunction()

if(APPLE)
    # 对项目用到的所有 Qt 目标执行剥离
    foreach(_qt_comp IN ITEMS Core Gui Widgets Network Qml)
        dgl_strip_agl_target(Qt${QT_VERSION_MAJOR}::${_qt_comp})
    endforeach()

    # 诊断输出：确认剥离后不再出现 AGL（便于在 CI 日志中核对）
    foreach(_qt_comp IN ITEMS Core Gui Widgets Network Qml)
        set(_t Qt${QT_VERSION_MAJOR}::${_qt_comp})
        if(TARGET ${_t})
            get_target_property(_ill ${_t} INTERFACE_LINK_LIBRARIES)
            get_target_property(_ild ${_t} IMPORTED_LINK_DEPENDENT_LIBRARIES_RELEASE)
            message(STATUS "[AGL-DIAG] ${_t} INTERFACE_LINK_LIBRARIES        = ${_ill}")
            message(STATUS "[AGL-DIAG] ${_t} IMPORTED_LINK_DEPENDENT_LIBRARIES_RELEASE = ${_ild}")
        endif()
    endforeach()

    # Qt 6.5 及更早版本仍需要 OpenGL.framework（与 AGL 无关，正常链接即可）
    find_library(OPENGL_FRAMEWORK OpenGL)
    if(OPENGL_FRAMEWORK)
        target_link_libraries(${PROJECT_NAME} PRIVATE "${OPENGL_FRAMEWORK}")
        message(STATUS "Linking explicitly against OpenGL.framework: ${OPENGL_FRAMEWORK}")
    else()
        message(WARNING "OpenGL.framework not found; Qt may need it at link time")
    endif()
endif()

# -------------------- 链接库 --------------------
target_link_libraries(${PROJECT_NAME}
    PUBLIC
        Qt::Core
        Qt::Gui
        Qt::Widgets
        Qt::Network
        Qt::Qml
    PRIVATE
        nlohmann_json
        Python::Python
)
target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_definitions(${PROJECT_NAME} PRIVATE PYBIND11_ASSERT_GIL_HELD_INCREF_DECREF)

# -------------------- 安装规则（供 CPack 使用）--------------------
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION .
    BUNDLE DESTINATION .
    LIBRARY DESTINATION .
)
install(DIRECTORY python/ DESTINATION python FILES_MATCHING PATTERN "*.py")
install(DIRECTORY config/ DESTINATION config FILES_MATCHING PATTERN "*.json")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/")
    install(DIRECTORY assets/ DESTINATION assets)
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/qcss/")
    install(DIRECTORY qcss/ DESTINATION qcss)
endif()

# -------------------- Qt 部署工具 --------------------
if(WIN32)
    get_target_property(_qt_qmake_location Qt${QT_VERSION_MAJOR}::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BINARY_DIR "${_qt_qmake_location}" DIRECTORY)
    set(QT_BINARY_DIR "${QT_BINARY_DIR}" CACHE INTERNAL "Qt binary directory")

    install(CODE [[
        message(STATUS "Running windeployqt for ${PROJECT_NAME}...")
        find_program(WINDEPLOYQT_EXECUTABLE
            NAMES windeployqt windeployqt6
            HINTS "${QT_BINARY_DIR}"
            REQUIRED
        )
        set(DEPLOY_TARGET "${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}${CMAKE_EXECUTABLE_SUFFIX}")
        execute_process(COMMAND "${WINDEPLOYQT_EXECUTABLE}"
            --release --no-translations --no-system-d3d-compiler --no-compiler-runtime
            "${DEPLOY_TARGET}"
            RESULT_VARIABLE _deploy_result
        )
        if(NOT _deploy_result EQUAL 0)
            message(FATAL_ERROR "windeployqt failed")
        endif()
    ]])
elseif(APPLE)
    # macdeployqt 在 CI 中手动执行
    message(STATUS "macOS: Qt deployment will be done by macdeployqt in CI")
else()
    message(STATUS "Linux: Qt deployment will be done by linuxdeployqt in CI")
endif()

# -------------------- Python 运行时和第三方包安装 --------------------
    if(WIN32)
        # 复制 Python 核心 DLL
        set(PYTHON_DLL_NAMES
            "python3.dll"
            "python${Python_VERSION_MAJOR}${Python_VERSION_MINOR}.dll"
        )
        foreach(DLL_NAME ${PYTHON_DLL_NAMES})
            find_file(PYTHON_DLL_PATH
                NAMES "${DLL_NAME}"
                HINTS ${Python_RUNTIME_LIBRARY_DIRS}
                      "${Python_EXECUTABLE}/.."
                      "${Python_EXECUTABLE}/../bin"
                DOC "Path to ${DLL_NAME}"
                REQUIRED
            )
            if(PYTHON_DLL_PATH)
                install(FILES "${PYTHON_DLL_PATH}" DESTINATION .)
                message(STATUS "Found Python DLL: ${PYTHON_DLL_PATH}")
            else()
                message(WARNING "Could not find ${DLL_NAME}, Python subprocess may fail")
            endif()
        endforeach()

        # 处理 Python 标准库（打包为 zip，安装时动态创建）
        if(WIN32 AND Python_STDLIB)
        set(PYTHON_ZIP_NAME "python${Python_VERSION_MAJOR}${Python_VERSION_MINOR}.zip")
        set(PYTHON_ZIP_PATH "${CMAKE_CURRENT_BINARY_DIR}/${PYTHON_ZIP_NAME}")

        # 若 zip 不存在，或是上次打包失败留下的 0 字节空文件，则重新打包
        set(_PYTHON_ZIP_NEED_CREATE TRUE)
        if(EXISTS "${PYTHON_ZIP_PATH}")
            file(SIZE "${PYTHON_ZIP_PATH}" _PYTHON_ZIP_SIZE)
            if(_PYTHON_ZIP_SIZE GREATER 0)
                set(_PYTHON_ZIP_NEED_CREATE FALSE)
                message(STATUS "Python standard library zip already exists, skipping creation: ${PYTHON_ZIP_PATH}")
            else()
                message(STATUS "Python standard library zip exists but is empty, recreating...")
            endif()
        endif()

        if(_PYTHON_ZIP_NEED_CREATE)
            # 在配置阶段创建 zip 文件
            message(STATUS "Creating Python standard library zip: ${PYTHON_ZIP_PATH}")
            file(TO_CMAKE_PATH "${Python_STDLIB}" PYTHON_STDLIB_CMAKE)

            if(CMAKE_HOST_WIN32)
                # 使用系统自带的 bsdtar（Windows 10+ 内置）打包，并排除以下内容：
                #  - site-packages：第三方包体积巨大（本机约 5GB），且已有独立的
                #    PYTHON_PACKAGES_DIR 安装通道，不应打进标准库 zip
                #  - __pycache__ / *.pyc：字节码缓存，运行时自动重建
                #  - test：测试用例，运行时不需要
                # --strip-components=1 去掉 tar 的 "./" 前缀，否则 zipimport 无法导入
                execute_process(
                    COMMAND tar --format=zip -cf "${PYTHON_ZIP_PATH}"
                            --strip-components=1
                            --exclude=site-packages
                            --exclude=__pycache__
                            --exclude=test
                            --exclude=*.pyc
                            -C "${PYTHON_STDLIB_CMAKE}" .
                    RESULT_VARIABLE _zip_result
                    ERROR_VARIABLE _zip_error
                    OUTPUT_VARIABLE _zip_output
                )
                if(NOT _zip_result EQUAL 0)
                    message(FATAL_ERROR "Failed to create Python standard library zip: ${_zip_error}")
                endif()
            else()
                # Linux/macOS 交叉打包 Windows 目标时使用 tar
                execute_process(
                    COMMAND ${CMAKE_COMMAND} -E tar cf "${PYTHON_ZIP_PATH}" --format=zip -- .
                    WORKING_DIRECTORY "${PYTHON_STDLIB_CMAKE}"
                    RESULT_VARIABLE _zip_result
                )
                if(NOT _zip_result EQUAL 0)
                    message(FATAL_ERROR "Failed to create Python standard library zip")
                endif()
            endif()
        endif()

        # 安装生成的 zip 文件
        install(FILES "${PYTHON_ZIP_PATH}" DESTINATION .)
    endif()

    # 复制 Python 二进制扩展模块（DLLs）
    get_filename_component(PYTHON_ROOT "${Python_EXECUTABLE}" DIRECTORY)
    set(PYTHON_DLLS_DIR "${PYTHON_ROOT}/DLLs")
    if(EXISTS "${PYTHON_DLLS_DIR}")
        install(DIRECTORY "${PYTHON_DLLS_DIR}/" DESTINATION "DLLs")
        message(STATUS "Python DLLs directory will be installed to DLLs/")
    else()
        message(WARNING "Python DLLs directory not found, binary extensions may be missing")
    endif()

elseif(APPLE)
    # macOS: 复制 libpython 动态库
    get_target_property(PYTHON_LIB_PATH Python::Python IMPORTED_LOCATION)
    if(PYTHON_LIB_PATH AND EXISTS "${PYTHON_LIB_PATH}")
        install(FILES "${PYTHON_LIB_PATH}" DESTINATION lib)
    else()
        message(WARNING "Could not find libpython.dylib, Python subprocess may fail")
    endif()
    if(Python_STDLIB)
        install(DIRECTORY "${Python_STDLIB}/"
            DESTINATION python/Lib
            FILES_MATCHING PATTERN "*.py"
            PATTERN "__pycache__" EXCLUDE
            PATTERN "test" EXCLUDE
        )
    endif()

else()
    # Linux: 复制 libpython.so
    get_target_property(PYTHON_LIB_PATH Python::Python IMPORTED_LOCATION)
    if(PYTHON_LIB_PATH AND EXISTS "${PYTHON_LIB_PATH}")
        install(FILES "${PYTHON_LIB_PATH}" DESTINATION lib)
    else()
        message(WARNING "Could not find libpython.so, Python subprocess may fail")
    endif()
    if(Python_STDLIB)
        install(DIRECTORY "${Python_STDLIB}/"
            DESTINATION python/Lib
            FILES_MATCHING PATTERN "*.py"
            PATTERN "__pycache__" EXCLUDE
            PATTERN "test" EXCLUDE
        )
    endif()
endif()

# 复制自定义 Python 脚本
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/python/
    DESTINATION python
    FILES_MATCHING PATTERN "*.py"
)

# 安装第三方 Python 包（从 CI 传入的 _packages 目录）
set(PYTHON_PACKAGES_DIR "" CACHE PATH "Directory containing Python packages to bundle")
if(PYTHON_PACKAGES_DIR AND EXISTS "${PYTHON_PACKAGES_DIR}")
    install(DIRECTORY "${PYTHON_PACKAGES_DIR}/"
        DESTINATION "python/Lib/site-packages"
        PATTERN "__pycache__" EXCLUDE
    )
    message(STATUS "Third-party packages installed to python/Lib/site-packages/")
endif()

# -------------------- CPack 打包配置 --------------------
include(InstallRequiredSystemLibraries)

set(CPACK_PACKAGE_NAME "DG-LAB-Client")
set(CPACK_PACKAGE_VENDOR "CrimsonSeraph")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Qt-based client for DG-lab with Python scripting")
set(CPACK_PACKAGE_VERSION_MAJOR "0")
set(CPACK_PACKAGE_VERSION_MINOR "6")
set(CPACK_PACKAGE_VERSION_PATCH "0")
set(CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
set(CPACK_PACKAGE_CONTACT "your-email@example.com")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")

if(WIN32)
    set(CPACK_GENERATOR "NSIS;ZIP")
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop;TGZ")
else()
    set(CPACK_GENERATOR "DEB;RPM;TGZ")
    set(CPACK_RPM_PACKAGE_REQUIRES "libc.so.6, libstdc++.so.6")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.17), libstdc++6 (>= 5)")
endif()

set(CPACK_NSIS_DISPLAY_NAME "DG-LAB Client")
set(CPACK_NSIS_PACKAGE_NAME "DG-LAB Client")

include(CPack)
