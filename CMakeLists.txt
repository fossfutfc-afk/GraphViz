cmake_minimum_required(VERSION 3.10)
project(GraphViz VERSION 1.1.1 LANGUAGES CXX)

# C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

# ── GUI 构建选项 ──
option(BUILD_GUI "Build with Qt6 GUI support" ON)

# ── GUI 前置配置 ──
if(BUILD_GUI)
    if(NOT CMAKE_PREFIX_PATH)
        message(FATAL_ERROR
            "BUILD_GUI=ON requires CMAKE_PREFIX_PATH to point to Qt6 installation.\n"
            "  Use cmake --preset gui, or set -DCMAKE_PREFIX_PATH=<qt_mingw_path>")
    endif()

    find_package(Qt6 REQUIRED COMPONENTS Widgets)
    set(CMAKE_AUTOMOC ON)
endif()

# ── 源文件 ──
set(CORE_SOURCES
    src/Graph.cpp
    src/GraphParser.cpp
    src/GraphAlgorithm.cpp
)

set(GUI_SOURCES
    src/gui/MainWindow.cpp
    src/gui/GraphWidget.cpp
    src/gui/GraphTextEdit.cpp
    src/gui/ForceLayout.cpp
)

add_executable(GraphViz WIN32
    src/main.cpp
    ${CORE_SOURCES}
)

target_include_directories(GraphViz PRIVATE include)

if(BUILD_GUI)
    target_sources(GraphViz PRIVATE ${GUI_SOURCES})
    target_include_directories(GraphViz PRIVATE src/gui)
    target_link_libraries(GraphViz PRIVATE Qt6::Widgets)
    target_compile_definitions(GraphViz PRIVATE BUILD_GUI=1)
    target_compile_definitions(GraphViz PRIVATE
        GRAPHVIZ_VERSION="${PROJECT_VERSION}")

    if(WIN32)
        add_custom_command(TARGET GraphViz POST_BUILD
            COMMAND "${CMAKE_PREFIX_PATH}/bin/windeployqt.exe"
                --no-translations
                "$<TARGET_FILE:GraphViz>"
            COMMENT "Deploying Qt runtime DLLs..."
        )
    endif()
endif()
