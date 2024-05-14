# Copyright (c) 2024 Advanced Micro Devices, Inc.
#
# This file is part of the AMD Render Pipeline Shaders SDK which is
# released under the MIT LICENSE.
#
# See file LICENSE.txt for full license details.

file (GLOB_RECURSE SrcFiles
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.inl"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
)

add_library( rps_visualizer STATIC ${SrcFiles} ${ImGuiSrcFiles} )


target_include_directories( rps_visualizer
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/
        ${CMAKE_CURRENT_SOURCE_DIR}/include/
        ${PROJECT_SOURCE_DIR}/tools/
        ${PROJECT_SOURCE_DIR}/tools/shared/
        ${PROJECT_SOURCE_DIR}/include/
        ${PROJECT_SOURCE_DIR}/src/
        ${PROJECT_SOURCE_DIR}/external/
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include/
)

target_link_libraries( rps_visualizer ImGui )

set_target_properties( rps_visualizer PROPERTIES
    FOLDER "${AppFolder}"
)
