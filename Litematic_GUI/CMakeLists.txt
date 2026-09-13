cmake_minimum_required(VERSION 3.16)
project(Litematic_GUI LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 QUIET COMPONENTS Widgets)
if(NOT Qt6_FOUND)
    find_package(Qt5 5.15 QUIET COMPONENTS Widgets)
    if(NOT Qt5_FOUND)
        message(FATAL_ERROR "Qt6 or Qt5 Widgets not found. Install Qt or set CMAKE_PREFIX_PATH.")
    endif()
    set(QT_LIBS Qt5::Widgets)
else()
    set(QT_LIBS Qt6::Widgets)
endif()

set(RETARGET_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(CONV_DIR ${RETARGET_ROOT}/Litematic_V7_To_V6)

add_executable(Litematic_GUI WIN32
    main.cpp
    MainWindow.cpp
    MainWindow.h
    ConvertWorker.cpp
    ConvertWorker.h
    icons.qrc
    ${CONV_DIR}/LitematicFileConversion.cpp
)

if(WIN32)
    target_sources(Litematic_GUI PRIVATE app.rc)
endif()

target_include_directories(Litematic_GUI PRIVATE
    ${CONV_DIR}
    ${RETARGET_ROOT}/Dependencies
)

if(NOT DEFINED COMMON_LIBS)
    # standalone build: pull in bundled deps
    add_subdirectory(${RETARGET_ROOT}/Dependencies/xxhash ${CMAKE_CURRENT_BINARY_DIR}/xxhash)
    add_subdirectory(${RETARGET_ROOT}/Dependencies/zlib ${CMAKE_CURRENT_BINARY_DIR}/zlib)
    set(COMMON_LIBS xxhash zlib)
endif()

target_link_libraries(Litematic_GUI PRIVATE
    ${QT_LIBS}
    ${COMMON_LIBS}
)

if(MSVC)
    target_compile_options(Litematic_GUI PRIVATE /utf-8)
endif()
