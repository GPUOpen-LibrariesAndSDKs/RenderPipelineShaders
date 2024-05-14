# Copyright (c) 2024 Advanced Micro Devices, Inc.
#
# This file is part of the AMD Render Pipeline Shaders SDK which is
# released under the MIT LICENSE.
#
# See file LICENSE.txt for full license details.

if ( ${RpsEnableImGui} )
    BuildFolderProperty( "external" AppFolder )

    set (ImGuiSources
        ${RpsImGui_DIR}/imgui.cpp
        ${RpsImGui_DIR}/imgui.h
        ${RpsImGui_DIR}/imgui_draw.cpp
        ${RpsImGui_DIR}/imgui_internal.h
        ${RpsImGui_DIR}/imgui_tables.cpp
        ${RpsImGui_DIR}/imgui_widgets.cpp)

    set(ImGuiWin32BackendSources
        ${RpsImGui_DIR}/backends/imgui_impl_win32.cpp
        ${RpsImGui_DIR}/backends/imgui_impl_win32.h)

    set(ImGuiVkBackendSources
        ${RpsImGui_DIR}/backends/imgui_impl_vulkan.cpp
        ${RpsImGui_DIR}/backends/imgui_impl_vulkan.h)

    set(ImGuiDX12BackendSources
        ${RpsImGui_DIR}/backends/imgui_impl_dx12.cpp
        ${RpsImGui_DIR}/backends/imgui_impl_dx12.h)

    set(ImGuiDX11BackendSources
        ${RpsImGui_DIR}/backends/imgui_impl_dx11.cpp
        ${RpsImGui_DIR}/backends/imgui_impl_dx11.h)

    add_library(ImGui STATIC ${ImGuiSources})
    set_target_properties( ImGui PROPERTIES FOLDER "${AppFolder}/imgui")
    target_include_directories(ImGui PUBLIC ${RpsImGui_DIR})

    if(${Vulkan_FOUND})
        add_library(ImGuiVkBackend STATIC ${ImGuiVkBackendSources})
        set_target_properties( ImGuiVkBackend PROPERTIES FOLDER "${AppFolder}/imgui")
        target_include_directories(ImGuiVkBackend PUBLIC
            ${Vulkan_INCLUDE_DIRS}
            ${RpsImGui_DIR})
    endif()

    if ( WIN32 )
        add_library(ImGuiWin32Backend STATIC ${ImGuiWin32BackendSources})
        set_target_properties( ImGuiWin32Backend PROPERTIES FOLDER "${AppFolder}/imgui")
        target_include_directories(ImGuiWin32Backend PUBLIC ${RpsImGui_DIR})

        add_library(ImGuiDX12Backend STATIC ${ImGuiDX12BackendSources})
        set_target_properties( ImGuiDX12Backend PROPERTIES FOLDER "${AppFolder}/imgui")
        target_include_directories(ImGuiDX12Backend PUBLIC ${RpsImGui_DIR})

        add_library(ImGuiDX11Backend STATIC ${ImGuiDX11BackendSources})
        set_target_properties( ImGuiDX11Backend PROPERTIES FOLDER "${AppFolder}/imgui")
        target_include_directories(ImGuiDX11Backend PUBLIC ${RpsImGui_DIR})
    endif()

endif()