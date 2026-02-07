cmake_minimum_required(VERSION 3.16)
project(DG-LAB-Client LANGUAGES CXX)

include(qt.cmake)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR}
    COMPONENTS
        Core
        Gui
        Widgets
)
qt_standard_project_setup()

set(PROJECT_SOURCES
    main.cpp
    DGLABClient.ui
    include/DGLABClient.h
    src/DGLABClient.cpp
    include/PyExecutor.h
    src/PyExecutor.cpp
    include/PyExecutor_impl.h
)

qt_add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

set_target_properties(${PROJECT_NAME}
    PROPERTIES
        WIN32_EXECUTABLE TRUE
)

find_package(Python 3.9 COMPONENTS Interpreter Development REQUIRED)

add_subdirectory(pybind11-3.0.1 pybind11)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        Qt::Core
        Qt::Gui
        Qt::Widgets
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        pybind11::embed
        Python::Python
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${PYTHON_INCLUDE_DIRS}
)