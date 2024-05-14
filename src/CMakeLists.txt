# Copyright (c) 2024 Advanced Micro Devices, Inc.
#
# This file is part of the AMD Render Pipeline Shaders SDK which is
# released under the MIT LICENSE.
#
# See file LICENSE.txt for full license details.

BuildFolderProperty( "modules" AppFolder )

AddModule( rps_core "${CMAKE_CURRENT_SOURCE_DIR}/core" "${AppFolder}" "" "" "" STATIC "" )
AddModule( rps_frontend "${CMAKE_CURRENT_SOURCE_DIR}/frontend" "${AppFolder}" "" "" "" STATIC "rps_core" )
AddModule( rps_runtime "${CMAKE_CURRENT_SOURCE_DIR}/runtime/common" "${AppFolder}" "" "" "" STATIC "rps_core" )

if ( WIN32 )
    AddModule( rps_runtime_d3d11 "${CMAKE_CURRENT_SOURCE_DIR}/runtime/d3d11" "${AppFolder}" "" "" "" STATIC "rps_core;rps_runtime" )
    AddModule( rps_runtime_d3d12 "${CMAKE_CURRENT_SOURCE_DIR}/runtime/d3d12" "${AppFolder}" "" "" "" STATIC "rps_core;rps_runtime" )
    if (RpsEnableDXAgilitySDK)
        target_include_directories( rps_runtime_d3d12 PUBLIC ${DX12AgilitySDK_INCLUDE_DIR} )
        target_compile_definitions( rps_runtime_d3d12 PUBLIC RPS_DX12_AGILITY_SDK_VER=${DXAgilitySDK_VERSION} )
    endif()
endif()

if (Vulkan_FOUND)
    AddModule( rps_runtime_vk "${CMAKE_CURRENT_SOURCE_DIR}/runtime/vk" "${AppFolder}" "${Vulkan_INCLUDE_DIRS};" "" "" STATIC "rps_core;rps_runtime" )
    AddModule( rps_runtime_vkdyn "${CMAKE_CURRENT_SOURCE_DIR}/runtime/vk" "${AppFolder}" "${Vulkan_INCLUDE_DIRS};" "" "RPS_VK_DYNAMIC_LOADING;VK_NO_PROTOTYPES" STATIC "rps_core;rps_runtime" )
endif( )
