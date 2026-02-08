cmake_minimum_required(VERSION 3.16)
project(DG-LAB-Client LANGUAGES CXX)

include(qt.cmake)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Qt 相关设置
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# 查找 Qt
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR}
    COMPONENTS
        Core
        Gui
        Widgets
)
qt_standard_project_setup()

# 项目源文件
set(PROJECT_SOURCES
    main.cpp
    include/DGLABClient.ui
    include/DGLABClient.h
    src/DGLABClient.cpp
    
    include/PyExecutor.h
    src/PyExecutor.cpp
    include/PyExecutor_impl.h
    
    include/PyThreadPoolExecutor.h
    src/PyThreadPoolExecutor.cpp
    include/PyThreadPoolExecutor_impl.h

    include/ConfigManager.h
    src/ConfigManager.cpp

    config/AllConfig.json

    include/json.hpp
)

# 创建可执行文件（使用通用函数）
qt_add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

# Windows 特定设置
if(WIN32)
    set_target_properties(${PROJECT_NAME}
        PROPERTIES
            WIN32_EXECUTABLE TRUE
    )
endif()

# 查找 Python
find_package(Python 3.9 COMPONENTS Interpreter Development REQUIRED)

# 添加 pybind11
add_subdirectory(pybind11-3.0.1 pybind11)

# 链接库
target_link_libraries(${PROJECT_NAME}
    PUBLIC
        Qt::Core
        Qt::Gui
        Qt::Widgets
    PRIVATE
        pybind11::embed
        Python::Python
)

# 包含目录
target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 复制 Python 模块
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory 
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/python_modules"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/python/main.py"
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/python_modules/main.py"
    COMMENT "Copying Python modules..."
)

# Windows 平台特定设置
if(WIN32)
    # 获取 Python 版本并复制正确的 DLL
    # 只在文件存在时才复制
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/include/main.py")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory 
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/python_modules"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/python/main.py"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/python_modules/main.py"
            COMMENT "Copying Python modules..."
        )
    endif()
    # 复制 Qt DLLs
    if(Qt${QT_VERSION_MAJOR}_DIR)
        get_target_property(QT_CORE_LOCATION Qt${QT_VERSION_MAJOR}::Core LOCATION)
        get_filename_component(QT_BIN_DIR "${QT_CORE_LOCATION}" DIRECTORY)
        
        # 复制必要的 Qt DLLs
        set(QT_DLLS 
            Qt${QT_VERSION_MAJOR}Core.dll
            Qt${QT_VERSION_MAJOR}Gui.dll
            Qt${QT_VERSION_MAJOR}Widgets.dll
        )
        
        foreach(QT_DLL ${QT_DLLS})
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${QT_BIN_DIR}/${QT_DLL}"
                    "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${QT_DLL}"
            )
        endforeach()
    endif()
endif()
