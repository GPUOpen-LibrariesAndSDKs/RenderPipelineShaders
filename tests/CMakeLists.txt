# Copyright (c) 2024 Advanced Micro Devices, Inc.
#
# This file is part of the AMD Render Pipeline Shaders SDK which is
# released under the MIT LICENSE.
#
# See file LICENSE.txt for full license details.

option( RPSL_DLL "Build RPSL DLLs" ON )
option( RpsEnableVulkanTests "Enable Vulkan Test" ON )

if ( NOT RpsEnableVulkan )
    set( RpsEnableVulkanTests OFF )
endif ( )

# Adding a test project
function( AddTestApp AppName TestSrcFile CompileFlags CompileDefinitions LinkFlags IncludeDirectories DependenciesString UseRpslDll UseVkDynLoading AppFolder )
    
    set( GeneratedSource "" )
    get_filename_component( TestSrcFileDir ${TestSrcFile} DIRECTORY )

    set( RpslFileName ${TestSrcFileDir}/${AppName}.rpsl )
    string(REGEX REPLACE "_vk$" "" SharedName ${AppName})
    string(REGEX REPLACE "_d3d12$" "" SharedName ${SharedName})
    string(REGEX REPLACE "_d3d11$" "" SharedName ${SharedName})
    if ( NOT EXISTS ${RpslFileName} )
        set( RpslFileName ${TestSrcFileDir}/${SharedName}.rpsl )
    endif()
    if ( EXISTS ${RpslFileName} AND NOT RpsOsSupportsRpslCompiler )
        message( WARNING "'${AppName}' target skipped as the RPSL compiler (rps-hlslc) does not support the current platform yet. This is required for compiling '${RpslFileName}'." )
        return()
    endif()
    if ( EXISTS ${RpslFileName} )
        if ( UseRpslDll )
            set( RpslcTargetName ${AppName}_dyn )
        elseif ( UseVkDynLoading)
            set( RpslcTargetName ${AppName}_dyn_load )
        else()
            set( RpslcTargetName ${AppName} )
        endif()

        CompileRpslDxc( ${RpslcTargetName} ${RpslFileName} GeneratedSource "Generated")
        source_group( TREE "${CMAKE_CURRENT_BINARY_DIR}/${RpslcTargetName}.tmp/Generated/" PREFIX "rps_generated" FILES ${GeneratedSource} )

        if ( UseRpslDll )
            set( RpslDllTarget ${SharedName}_rpsl_dll )
            if ( NOT TARGET ${RpslDllTarget} )
                add_library( ${RpslDllTarget} SHARED ${RpslFileName} ${GeneratedSource} ${PROJECT_SOURCE_DIR}/src/runtime/common/rps_rpsl_host_dll.c )
                target_compile_definitions( ${RpslDllTarget} PRIVATE RPS_SHADER_GUEST=1 )
                set_target_properties( ${RpslDllTarget} PROPERTIES
                    OUTPUT_NAME ${SharedName}_rpsl
                    FOLDER "${AppFolder}/rpsl_dlls" )
            endif()
        else()
            set( TestSrcFile ${TestSrcFile} ${RpslFileName} )
        endif()
    elseif( UseRpslDll )
        return()
    endif()
    set( SharedHeaderName ${TestSrcFileDir}/${SharedName}_shared.hpp)
    if ( EXISTS ${SharedHeaderName} )
        set( TestSrcFile ${TestSrcFile} ${SharedHeaderName} )
    endif()
    if ( EXISTS "${TestSrcFileDir}/${AppName}_c.c" )
        set( TestSrcFile ${TestSrcFile} ${TestSrcFileDir}/${AppName}_c.c )
    endif( )

    if ( UseRpslDll )
        set( AppName ${AppName}_dyn )
        add_executable( ${AppName} WIN32 ${TestSrcFile} )
        target_compile_definitions(${AppName} PRIVATE USE_RPSL_DLL=1)
        set( DependenciesString "${DependenciesString};${RpslDllTarget}" )
    elseif( UseVkDynLoading )
        set( AppName ${AppName}_dyn_load )
        add_executable( ${AppName} WIN32 ${TestSrcFile} ${GeneratedSource} )
        target_compile_definitions(${AppName} PRIVATE RPS_VK_DYNAMIC_LOADING VK_NO_PROTOTYPES)
    else()
        add_executable( ${AppName} WIN32 ${TestSrcFile} ${GeneratedSource} )
    endif()

    if ( ${AppName} MATCHES "_vk$|_vk_dyn$|_vk_dyn_load$" )
        CopySpirvDXC( ${AppName} )
    endif()

    if ( WIN32 AND ( ${AppName} MATCHES "_d3d12$|_d3d12_dyn$" ) )
        CopyDXC( ${AppName} )
        CopyDX12AgilitySDKBinaries( ${AppName} )
    endif()

    target_include_directories( ${AppName} PRIVATE
        ${PROJECT_SOURCE_DIR}/include/
        ${PROJECT_SOURCE_DIR}/src/
        ${PROJECT_SOURCE_DIR}/tools/
        ${CMAKE_CURRENT_SOURCE_DIR}/../../external/catch2/single_include/
        ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/
        ${SrcFolder}/
        ${SrcInclude}
        ${IncludeDirectories}
    )
    set( DependencyList ${DependenciesString} )
    target_link_libraries( ${AppName} ${DependencyList} )

    if ( ${CompileFlags} MATCHES "CONSOLE" )
        set( LinkFlags /SUBSYSTEM:CONSOLE )
    endif( )

    target_compile_definitions(${AppName} PRIVATE ${CompileDefinitions})

    set_target_properties( ${AppName} PROPERTIES
        COMPILE_FLAGS "${CompileFlags}"
        FOLDER "${AppFolder}"
        LINK_FLAGS "${LinkFlags}"
        VS_DPI_AWARE "PerMonitor")

    # Convert test executable absolute paths to relative
    get_target_property(AppOutDir ${AppName} RUNTIME_OUTPUT_DIRECTORY)
    set(AppAbsoluteExecPath ${AppOutDir}/$<TARGET_FILE_NAME:${AppName}>)
    file(RELATIVE_PATH TestExecPath ${CMAKE_CURRENT_BINARY_DIR} ${AppAbsoluteExecPath})
    set(TestExecPath "./${TestExecPath}")

    if ( NOT ( ${AppName} MATCHES "_vk$|_vk_dyn$|_vk_dyn_load$" AND NOT ${RpsEnableVulkanTests}) )
        add_test( NAME ${AppName} COMMAND ${TestExecPath} --reporter junit --out test_report_${AppName}.xml )
    endif()

endfunction()

function ( SetupTestApp TestSrc AppFolder AdditionalIncludeDirectiories AdditionalDependecies UseRpslDll UseVkDynLoading)
    get_filename_component( TargetName ${TestSrc} NAME_WE )
    set(RpsApiRuntime "")
    foreach(ExcludeEntry IN LISTS ExcludeTests)
        if (TargetName MATCHES ${ExcludeEntry})
            set(TargetName "")
        endif()
    endforeach()

    if (TargetName MATCHES "_d3d12$")
        set(RpsApiRuntime "rps_runtime_d3d12;")
    elseif (TargetName MATCHES "_d3d11$")
        set(RpsApiRuntime "rps_runtime_d3d11;")
    elseif (TargetName MATCHES "_vk$")
        if (${Vulkan_FOUND})
            if(${UseVkDynLoading})
                set(RpsApiRuntime "rps_runtime_vkdyn;")
                set(CompileDefinitions "RPS_VK_DYNAMIC_LOADING;VK_NO_PROTOTYPES")
            else()
                set(RpsApiRuntime "rps_runtime_vk;${Vulkan_LIBRARY}")
            endif()
            
            set(VulkanIncDir ${Vulkan_INCLUDE_DIRS})
        else()
            set(TargetName "")
        endif()
    endif()
    set( CompileFlags "" )
    if (MSVC)
        set( CompileFlags "${CompileFlags} /DCONSOLE" )
    endif()
    if (NOT ${TargetName} STREQUAL "")
        set( Dependencies "rps_core;rps_frontend;rps_runtime;${RpsApiRuntime};${AdditionalDependecies}")
        set( IncludeDirectories "${AdditionalIncludeDirectiories}")
        if (${Vulkan_FOUND})
            set( IncludeDirectories "${VulkanIncDir};${IncludeDirectories}")
        endif()
        AddTestApp( ${TargetName} ${TestSrc} "${CompileFlags}" "${CompileDefinitions}" "" "${IncludeDirectories}" "${Dependencies}" False False ${AppFolder} )
        if ( ${RPSL_DLL} AND UseRpslDll )
            AddTestApp( ${TargetName} ${TestSrc} "${CompileFlags}" "${CompileDefinitions}" "" "${IncludeDirectories}" "${Dependencies}" True False ${AppFolder} )
        endif()
        if( ${UseVkDynLoading} AND TargetName MATCHES "_vk$" )
            AddTestApp( ${TargetName} ${TestSrc} "${CompileFlags}" "${CompileDefinitions}" "" "${IncludeDirectories}" "${Dependencies}" False True ${AppFolder} )
        endif()
    endif()
endfunction( )

BuildFolderProperty( "tests" AppFolder )

add_subdirectory(utils)
add_subdirectory(console)
add_subdirectory(gui)
