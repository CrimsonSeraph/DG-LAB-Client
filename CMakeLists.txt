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

# pybind11：用于 C++/Python 互操作
FetchContent_Declare(
  pybind11
  GIT_REPOSITORY https://github.com/pybind/pybind11.git
  GIT_TAG v2.13.6
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(pybind11)

# -------------------- 手动获取 nlohmann/json 单头文件 --------------------
# 定义 json.hpp 的目标路径
set(NLOHMANN_JSON_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/include")
set(NLOHMANN_JSON_HEADER "${NLOHMANN_JSON_INCLUDE_DIR}/nlohmann/json.hpp")

# 如果文件不存在，则下载
if(NOT EXISTS "${NLOHMANN_JSON_HEADER}")
    message(STATUS "Downloading json.hpp from GitHub...")
    # 确保目录存在
    file(MAKE_DIRECTORY "${NLOHMANN_JSON_INCLUDE_DIR}/nlohmann")
    # 下载文件（显示进度）
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

# 创建一个接口库，只提供头文件路径
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE "${NLOHMANN_JSON_INCLUDE_DIR}")

# -------------------- 查找 Qt --------------------
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets)

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

    # 资源文件
    DGLABClient.qrc

    # Python 解释器管理器
    include/PyExecutorManager.h
    src/PyExecutorManager.cpp
    include/PyExecutorManager_impl.hpp
    # Python 解释器
    include/PyExecutor.h
    src/PyExecutor.cpp
    include/PyExecutor_impl.hpp
    # Python 解释器（进程池版）
    include/PyThreadPoolExecutor.h
    src/PyThreadPoolExecutor.cpp
    include/PyThreadPoolExecutor_impl.hpp

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
    # 默认配置
    include/DefaultConfigs.h
    src/DefaultConfigs.cpp

    # 控制台
    include/Console.h
    src/Console.cpp

    # Debug 日志
    include/DebugLog.h
    src/DebugLog.cpp

    # 配置文件
    config/main.json
    config/system.json
    config/user.json
)

# -------------------- 创建可执行文件 --------------------
if (QT_VERSION_MAJOR EQUAL 6)
    qt_add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})
else()
    add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})
endif()

# 显式设置 macOS Bundle 属性
if(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE TRUE)
endif()

# Windows 特定设置：让程序以窗口方式运行（无控制台）
if(WIN32)
    set_target_properties(${PROJECT_NAME}
        PROPERTIES
            WIN32_EXECUTABLE TRUE
    )
endif()

# 复制配置文件
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/config"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/config"
    COMMENT "Copying configuration files to output directory"
)

# 复制 Python 脚本
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/python"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/python"
    COMMENT "Copying Python scripts to output directory"
)

# 复制 assets 资源
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
        COMMENT "Copying assets to output directory"
    )
endif()

# 复制 qcss 样式表
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/qcss/")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/qcss"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/qcss"
        COMMENT "Copying qcss files to output directory"
    )
endif()

# -------------------- 查找 Python --------------------
# 要求 Python 3.9+，需要解释器和开发库
find_package(Python 3.9 COMPONENTS Interpreter Development REQUIRED)

# -------------------- 链接库 --------------------
target_link_libraries(${PROJECT_NAME}
    PUBLIC
        Qt::Core
        Qt::Gui
        Qt::Widgets
    PRIVATE
        pybind11::embed                 # pybind11 嵌入模式
        nlohmann_json                   # JSON 库
        Python::Python                  # Python 库
)

# 包含头文件目录
target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# -------------------- 安装规则（供 CPack 使用）--------------------
# 安装可执行文件
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin      # Windows 和 Linux 的可执行文件
    BUNDLE DESTINATION .          # macOS 的 .app 包
    LIBRARY DESTINATION lib       # 动态库
)

# 安装 Python 脚本（所有 .py 文件）
install(DIRECTORY python/
    DESTINATION python
    FILES_MATCHING PATTERN "*.py"
)

# 安装配置文件（所有 .json 文件）
install(DIRECTORY config/
    DESTINATION config
    FILES_MATCHING PATTERN "*.json"
)

# 安装 assets 资源文件（如图标、图片等）
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/")
    install(DIRECTORY assets/ DESTINATION assets)
endif()

# 安装 qcss 样式表文件
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/qcss/")
    install(DIRECTORY qcss/ DESTINATION qcss)
endif()

# -------------------- Qt 部署工具（自动复制 Qt DLL/框架）--------------------
if(WIN32)
    # 获取 Qt 的 bin 目录（从 Qt::qmake 目标中获取）
    get_target_property(_qt_qmake_location Qt${QT_VERSION_MAJOR}::qmake IMPORTED_LOCATION)
    message(STATUS "Qt qmake location: ${_qt_qmake_location}")
    get_filename_component(QT_BINARY_DIR "${_qt_qmake_location}" DIRECTORY)
    message(STATUS "QT_BINARY_DIR: ${QT_BINARY_DIR}")
    # 将路径保存为缓存变量，供安装脚本使用
    set(QT_BINARY_DIR "${QT_BINARY_DIR}" CACHE INTERNAL "Qt binary directory")

    install(CODE [[
        message(STATUS "Running windeployqt for ${PROJECT_NAME}...")
        # 在安装时查找 windeployqt（使用缓存的 QT_BINARY_DIR）
        find_program(WINDEPLOYQT_EXECUTABLE
            NAMES windeployqt windeployqt6
            HINTS "${QT_BINARY_DIR}"
            REQUIRED
        )
        message(STATUS "Found windeployqt: ${WINDEPLOYQT_EXECUTABLE}")
        set(DEPLOY_TARGET "${CMAKE_INSTALL_PREFIX}/bin/${PROJECT_NAME}${CMAKE_EXECUTABLE_SUFFIX}")
        execute_process(COMMAND "${WINDEPLOYQT_EXECUTABLE}"
            --release
            --no-translations
            --no-system-d3d-compiler
            --no-compiler-runtime
            --dir "${CMAKE_INSTALL_PREFIX}/bin"
            "${DEPLOY_TARGET}"
            WORKING_DIRECTORY "${CMAKE_INSTALL_PREFIX}/bin"
            RESULT_VARIABLE _deploy_result
        )
        if(NOT _deploy_result EQUAL 0)
            message(FATAL_ERROR "windeployqt failed!")
        endif()
    ]])
elseif(APPLE)
    find_program(MACDEPLOYQT_EXECUTABLE macdeployqt HINTS "${QT_BINARY_DIR}" REQUIRED)
    # 由 macdeployqt 处理 Qt 依赖将在 CI 中完成，此处仅提示
elseif(UNIX AND NOT APPLE)
    # Linux 下 Qt 依赖处理将在 CI 中使用 linuxdeployqt 完成，此处仅提示
    message(STATUS "Linux: Qt dependencies will be handled by linuxdeployqt in CI.")
endif()

# -------------------- Python 运行时和包依赖处理 --------------------
# 在安装阶段复制 Python 共享库和标准库
# 注意：这会显著增大安装包体积，可根据实际需求裁剪标准库

if(WIN32)
    # Windows：复制 Python DLL（python3.dll 和 python3X.dll）
    if(DEFINED Python_RUNTIME_LIBRARY_DIRS)
        foreach(_dir ${Python_RUNTIME_LIBRARY_DIRS})
            install(FILES
                "${_dir}/python3.dll"
                "${_dir}/python${Python_VERSION_MAJOR}${Python_VERSION_MINOR}.dll"
                DESTINATION bin
                OPTIONAL
            )
        endforeach()
    else()
        get_filename_component(PYTHON_BIN_DIR "${Python_EXECUTABLE}" DIRECTORY)
        install(FILES
            "${PYTHON_BIN_DIR}/python3.dll"
            "${PYTHON_BIN_DIR}/python${Python_VERSION_MAJOR}${Python_VERSION_MINOR}.dll"
            DESTINATION bin
            OPTIONAL
        )
    endif()

    # 复制 Python 标准库（整个 Lib 目录，可排除测试模块）
    if(Python_STDLIB)
        install(DIRECTORY "${Python_STDLIB}/"
            DESTINATION python/Lib
            FILES_MATCHING PATTERN "*.py"
            PATTERN "__pycache__" EXCLUDE
            PATTERN "test" EXCLUDE
        )
    endif()

elseif(APPLE)
    # macOS：复制 libpython 动态库
    get_target_property(PYTHON_LIB_PATH Python::Python IMPORTED_LOCATION)
    if(PYTHON_LIB_PATH)
        install(FILES "${PYTHON_LIB_PATH}" DESTINATION lib)
    endif()
    # 复制标准库
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
    if(PYTHON_LIB_PATH)
        install(FILES "${PYTHON_LIB_PATH}" DESTINATION lib)
    endif()
    # 复制标准库
    if(Python_STDLIB)
        install(DIRECTORY "${Python_STDLIB}/"
            DESTINATION python/Lib
            FILES_MATCHING PATTERN "*.py"
            PATTERN "__pycache__" EXCLUDE
            PATTERN "test" EXCLUDE
        )
    endif()
endif()

# 安装 Python 第三方包（如 websockets）
set(PYTHON_PACKAGES_DIR "" CACHE PATH "Directory containing Python packages to bundle")
if(PYTHON_PACKAGES_DIR AND EXISTS "${PYTHON_PACKAGES_DIR}")
    install(DIRECTORY "${PYTHON_PACKAGES_DIR}/"
        DESTINATION python/Lib/site-packages
        FILES_MATCHING PATTERN "*"
    )
endif()

# -------------------- CPack 打包配置 --------------------
include(InstallRequiredSystemLibraries)

set(CPACK_PACKAGE_NAME "DG-LAB-Client")
set(CPACK_PACKAGE_VENDOR "CrimsonSeraph")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Qt-based client for DG-Lab with Python scripting")
set(CPACK_PACKAGE_VERSION_MAJOR "0")
set(CPACK_PACKAGE_VERSION_MINOR "1")
set(CPACK_PACKAGE_VERSION_PATCH "0")
set(CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
set(CPACK_PACKAGE_CONTACT "your-email@example.com")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt")

# 根据平台选择默认生成器
if(WIN32)
    set(CPACK_GENERATOR "NSIS;ZIP")
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop;TGZ")
else()
    # Linux 下生成 DEB、RPM、TGZ 以及 AppImage（将在 CI 中通过 linuxdeployqt 生成）
    set(CPACK_GENERATOR "DEB;RPM;TGZ")
endif()

# 特定平台的额外设置
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6, libstdc++6")
set(CPACK_NSIS_DISPLAY_NAME "DG-LAB Client")
set(CPACK_NSIS_PACKAGE_NAME "DG-LAB Client")

include(CPack)
