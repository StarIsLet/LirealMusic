# SPDX-License-Identifier: AGPL-3.0-or-later
# Lireal Music - C++ audio visual rendering engine.
# Copyright (C) 2026 Lireal contributors
#
# This file is part of Lireal Music.
# Lireal Music is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

cmake_minimum_required(VERSION 3.22)

project(LirealMusic
    VERSION 0.1.0
    DESCRIPTION "Lireal Linux C++ audio visual rendering engine"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(LIREAL_BUILD_GUI "Build the Qt dashboard application" ON)
option(LIREAL_ENABLE_OPENMP "Enable OpenMP acceleration when available" ON)
option(LIREAL_ENABLE_ONNXRUNTIME "Enable ONNX Runtime neural source separation when available" ON)
option(LIREAL_ENABLE_WEBGPU "Enable experimental WebGPU backend detection hooks" ON)

find_package(PkgConfig REQUIRED)
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs videoio)
find_package(FFTW3f QUIET)
find_package(Qt6 QUIET COMPONENTS Widgets Concurrent Gui)

pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
    libavformat
    libavcodec
    libavutil
    libswresample
    libswscale
)

if(LIREAL_ENABLE_ONNXRUNTIME)
    pkg_check_modules(ONNXRUNTIME QUIET IMPORTED_TARGET onnxruntime)
endif()

if(LIREAL_ENABLE_OPENMP)
    find_package(OpenMP QUIET)
endif()

set(LIREAL_HEADERS
    include/lireal/audio/audio_analyzer.hpp
    include/lireal/audio/onnx_source_separator.hpp
    include/lireal/lyrics/lrc_parser.hpp
    include/lireal/render/render_config.hpp
    include/lireal/render/video_renderer.hpp
    include/lireal/system/hardware_profile.hpp
)

set(LIREAL_SOURCES
    src/lireal/audio/audio_analyzer.cpp
    src/lireal/audio/onnx_source_separator.cpp
    src/lireal/lyrics/lrc_parser.cpp
    src/lireal/render/video_renderer.cpp
    src/lireal/system/hardware_profile.cpp
)

add_library(lireal_core STATIC ${LIREAL_HEADERS} ${LIREAL_SOURCES})
target_include_directories(lireal_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(lireal_core
    PUBLIC
        ${OpenCV_LIBS}
    Qt6::Gui
        PkgConfig::FFMPEG
)

    add_executable(lireal_tui src/tui_main.cpp)
    target_link_libraries(lireal_tui PRIVATE lireal_core)

if(FFTW3f_FOUND)
    target_link_libraries(lireal_core PUBLIC FFTW3::fftw3f)
    target_compile_definitions(lireal_core PUBLIC LIREAL_HAS_FFTW3=1)
else()
    target_compile_definitions(lireal_core PUBLIC LIREAL_HAS_FFTW3=0)
endif()

if(ONNXRUNTIME_FOUND)
    target_link_libraries(lireal_core PUBLIC PkgConfig::ONNXRUNTIME)
    target_compile_definitions(lireal_core PUBLIC LIREAL_HAS_ONNXRUNTIME=1)
else()
    target_compile_definitions(lireal_core PUBLIC LIREAL_HAS_ONNXRUNTIME=0)
endif()

if(OpenMP_CXX_FOUND)
    target_link_libraries(lireal_core PUBLIC OpenMP::OpenMP_CXX)
    target_compile_definitions(lireal_core PUBLIC LIREAL_HAS_OPENMP=1)
else()
    target_compile_definitions(lireal_core PUBLIC LIREAL_HAS_OPENMP=0)
endif()

if(LIREAL_ENABLE_WEBGPU)
    target_compile_definitions(lireal_core PUBLIC LIREAL_HAS_WEBGPU=0)
else()
    target_compile_definitions(lireal_core PUBLIC LIREAL_HAS_WEBGPU=0)
endif()

if(LIREAL_BUILD_GUI)
    if(NOT Qt6_FOUND)
        message(FATAL_ERROR "Qt6 Widgets/Concurrent not found. Install qt6-base-dev or configure with -DLIREAL_BUILD_GUI=OFF.")
    endif()

    add_executable(lireal
        src/main.cpp
        include/lireal/ui/dashboard_window.hpp
        src/lireal/ui/dashboard_window.cpp
    )
    target_link_libraries(lireal PRIVATE lireal_core Qt6::Widgets Qt6::Concurrent)
    target_compile_definitions(lireal PRIVATE LIREAL_THEME_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/themes/sakura.qss")
endif()
