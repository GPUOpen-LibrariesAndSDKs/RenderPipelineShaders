# Copyright (c) 2024 Advanced Micro Devices, Inc.
#
# This file is part of the AMD Render Pipeline Shaders SDK which is
# released under the MIT LICENSE.
#
# See file LICENSE.txt for full license details.

set( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE} )

file( GLOB_RECURSE GuiTestSrcs "${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp" )

if ( NOT RpsEnableVulkanTests )
    list( FILTER GuiTestSrcs EXCLUDE REGEX ".*_vk.cpp" )
endif()

message( STATUS "Found Tests: ${GuiTestSrcs}" )

# TODO: 
if ( WIN32 )
    foreach( TestSrc ${GuiTestSrcs} )
        if( TestSrc MATCHES "${CMAKE_CURRENT_SOURCE_DIR}/test_visualizer" )
            if ( ${RpsEnableImGui} )
                set( Dependencies "rps_visualizer;ImGuiWin32Backend" )
                set( IncludeDirectories ${CMAKE_SOURCE_DIR}/tools/rps_visualizer/include )
                if( TestSrc MATCHES "_vk.cpp" )
                    set( Dependencies "${Dependencies};ImGuiVkBackend" )
                else()
                    set( Dependencies "${Dependencies};ImGuiDX12Backend" )
                endif()
                SetupTestApp( ${TestSrc} "${AppFolder}/gui" "${IncludeDirectories}" "${Dependencies}" False False)
            endif()
        elseif(TestSrc MATCHES "${CMAKE_CURRENT_SOURCE_DIR}/test_triangle_vk")
            SetupTestApp( ${TestSrc} "${AppFolder}/gui" "" "" True True )
        else()
            SetupTestApp( ${TestSrc} "${AppFolder}/gui" "" "" False False)
        endif()
    endforeach()
endif()