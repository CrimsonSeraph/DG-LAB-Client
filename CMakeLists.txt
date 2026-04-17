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

if(NOT EXISTS "${NLOHMANN_JSON_HEADER}")
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
else()
    message(STATUS "json.hpp already exists, skipping download")
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
    include/Console.h
    src/Console.cpp

    # 波形绘制
    include/SampledWaveformWidget.h
    src/SampledWaveformWidget.cpp

    # Debug 日志
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

        if(EXISTS "${PYTHON_ZIP_PATH}")
            message(STATUS "Python standard library zip already exists, skipping creation: ${PYTHON_ZIP_PATH}")
        else()
            # 在配置阶段创建 zip 文件
            message(STATUS "Creating Python standard library zip: ${PYTHON_ZIP_PATH}")
            file(TO_CMAKE_PATH "${Python_STDLIB}" PYTHON_STDLIB_CMAKE)

            # 使用 CMake 的 file(WRITE) 和 execute_process 在配置时执行打包
            if(CMAKE_HOST_WIN32)
                execute_process(
                    COMMAND powershell -Command "Compress-Archive -Path '*' -DestinationPath '${PYTHON_ZIP_PATH}' -Force"
                    WORKING_DIRECTORY "${PYTHON_STDLIB_CMAKE}"
                    RESULT_VARIABLE _zip_result
                    ERROR_VARIABLE _zip_error
                    OUTPUT_VARIABLE _zip_output
                )
                if(NOT _zip_result EQUAL 0)
                    message(FATAL_ERROR "Failed to create Python standard library zip: ${_zip_error}")
                endif()
            else()
                # Linux/macOS 使用 tar
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
    # macOS：复制 libpython 动态库
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
    # Linux：复制 libpython.so
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
set(CPACK_PACKAGE_VERSION_MINOR "3")
set(CPACK_PACKAGE_VERSION_PATCH "0")
set(CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
set(CPACK_PACKAGE_CONTACT "your-email@example.com")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt")

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
