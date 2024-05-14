# Copyright (c) 2024 Advanced Micro Devices, Inc.
#
# This file is part of the AMD Render Pipeline Shaders SDK which is
# released under the MIT LICENSE.
#
# See file LICENSE.txt for full license details.

if ( NOT RpsJITSupported )
    # rpsl-explorer requires JIT
    return()
endif ( )

set(AppName rpsl_explorer)

file (GLOB_RECURSE SrcFiles
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.inl"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hlsl"
)

set(LibDependencies "rps_runtime_d3d12;ImGui;ImGuiWin32Backend;ImGuiDX12Backend")
add_compile_definitions(RPS_D3D12_RUNTIME=1)

if (${Vulkan_FOUND})
    set(LibDependencies "${LibDependencies};rps_runtime_vk;${Vulkan_LIBRARY};ImGuiVkBackend")
    set(VulkanIncDir ${Vulkan_INCLUDE_DIRS})
    add_compile_definitions(RPS_VK_RUNTIME=1)
endif()

set(LibDependencies "${LibDependencies};rps_visualizer")

add_executable( ${AppName} WIN32 ${SrcFiles} )

CopyJITCompiler( ${AppName} )
CopyDX12AgilitySDKBinaries( ${AppName} )

target_include_directories( ${AppName} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/
    ${PROJECT_SOURCE_DIR}/tools/
    ${PROJECT_SOURCE_DIR}/tools/shared
    ${PROJECT_SOURCE_DIR}/include/
    ${PROJECT_SOURCE_DIR}/external/
    ${PROJECT_SOURCE_DIR}/external/imgui
)

if (${Vulkan_FOUND})
    target_include_directories( ${AppName} PRIVATE ${VulkanIncDir})
endif()

target_link_libraries( ${AppName} ${LibDependencies} ImGui)

if ( ${CompileFlags} MATCHES "CONSOLE" )
    set( LinkFlags /SUBSYSTEM:CONSOLE )
endif( )

set_target_properties( ${AppName} PROPERTIES
    CXX_STANDARD 17
    COMPILE_FLAGS "${CompileFlags}"
    FOLDER "${AppFolder}"
    LINK_FLAGS "${LinkFlags}"
    VS_DPI_AWARE "PerMonitor" )
