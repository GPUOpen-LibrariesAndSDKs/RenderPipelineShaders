# Copyright (c) 2022 Advanced Micro Devices, Inc. All rights reserved.
#
# This file is part of the AMD Render Pipeline Shaders SDK which is
# released under the AMD INTERNAL EVALUATION LICENSE.
#
# See file LICENSE.RTF for full license details.

BuildFolderProperty( "modules" AppFolder )

AddModule( rps_core "${CMAKE_CURRENT_SOURCE_DIR}/core" "${AppFolder}" "" "" STATIC "" )
AddModule( rps_frontend "${CMAKE_CURRENT_SOURCE_DIR}/frontend" "${AppFolder}" "" "" STATIC "rps_core" )
AddModule( rps_runtime "${CMAKE_CURRENT_SOURCE_DIR}/runtime/common" "${AppFolder}" "" "" STATIC "rps_core" )
AddModule( rps_runtime_d3d12 "${CMAKE_CURRENT_SOURCE_DIR}/runtime/d3d12" "${AppFolder}" "" "" STATIC "rps_core;rps_runtime" )
if (RpsEnableDXAgilitySDK)
    target_include_directories( rps_runtime_d3d12 PUBLIC ${DX12AgilitySDK_INCLUDE_DIR} )
    target_compile_definitions( rps_runtime_d3d12 PUBLIC RPS_DX12_AGILITY_SDK_VER=${DXAgilitySDK_VERSION} )
endif()
AddModule( rps_runtime_d3d11 "${CMAKE_CURRENT_SOURCE_DIR}/runtime/d3d11" "${AppFolder}" "" "" STATIC "rps_core;rps_runtime" )
if (Vulkan_FOUND)
    AddModule( rps_runtime_vk "${CMAKE_CURRENT_SOURCE_DIR}/runtime/vk" "${AppFolder}" "${Vulkan_INCLUDE_DIRS};" "" STATIC "rps_core;rps_runtime" )
endif( )
