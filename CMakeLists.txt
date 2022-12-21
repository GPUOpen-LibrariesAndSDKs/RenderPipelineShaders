# Copyright (c) 2022 Advanced Micro Devices, Inc. All rights reserved.
#
# This file is part of the AMD Render Pipeline Shaders SDK which is
# released under the AMD INTERNAL EVALUATION LICENSE.
#
# See file LICENSE.RTF for full license details.

cmake_minimum_required(VERSION 3.12.1)

option( RpsBuildTests "Enable unit test targets" ON )
option( RpsBuildTools "Enable tool targets" ON )
option( RpsEnableVulkan "Enable Vulkan backend" ON )
option( RpsEnableImGui "Enable ImGui" ON)
option( RpsEnableDXAgilitySDK "Enable DX12 Agility SDK" OFF )

if ( "${CMAKE_GENERATOR_PLATFORM}" STREQUAL "" )
    project( "rps" )
else ( )
    message( STATUS "Generator platform: ${CMAKE_GENERATOR_PLATFORM}" )
    project( "rps_${CMAKE_GENERATOR_PLATFORM}" )
endif ( )

set( NugetPackagesRoot "${CMAKE_BINARY_DIR}/rps_nuget_packages" )
set( DXAgilitySDK_VERSION_STRING "1.706.3-preview" )
set( DXAgilitySDK_VERSION 706 ) # to set D3D12SDKVersion
set( DXAgilitySDK_INSTALL_DIR "${NugetPackagesRoot}/DXAgilitySDK.${DXAgilitySDK_VERSION_STRING}" )
set( RpsDXAgilitySDK_DIR "${DXAgilitySDK_INSTALL_DIR}" CACHE STRING "DX12 Agility SDK directory" )

set( RpsRootSolutionFolder "" CACHE STRING "Root IDE solution folder" )
set( RpsImGui_DIR "${PROJECT_SOURCE_DIR}/external/imgui" CACHE STRING "ImGui source directory" )

function( BuildFolderProperty RelativeFolder OutputVar )
    if ( "${RpsRootSolutionFolder}" STREQUAL "" )
        set(${OutputVar} "${RelativeFolder}" PARENT_SCOPE)
    else()
        set(${OutputVar} "${RpsRootSolutionFolder}/${RelativeFolder}" PARENT_SCOPE)
    endif()
endfunction()

if ( "${CMAKE_GENERATOR_PLATFORM}" STREQUAL "Arm64" )
    set( RpsEnableVulkan OFF )
endif ( )

if ( RpsEnableVulkan )
    find_package( Vulkan 1.2 )
endif ( )

if ( NOT WIN32 )
    set ( RpsEnableDXAgilitySDK OFF )
endif()

# JIT only supports win64 for now
set ( RpsJITSupported FALSE )
if ( WIN32 AND (CMAKE_SIZEOF_VOID_P EQUAL 8) )
    set ( RpsJITSupported TRUE )
endif ( )

if ( RpsEnableDXAgilitySDK )

    set( DX12AgilitySDK_INCLUDE_DIR "${RpsDXAgilitySDK_DIR}/build/native/include" )

    # fallback.
    if ( (NOT "${RpsDXAgilitySDK_DIR}" STREQUAL "${DXAgilitySDK_INSTALL_DIR}") AND (NOT EXISTS "${DX12AgilitySDK_INCLUDE_DIR}") )
        set( DX12AgilitySDK_DIR "${DXAgilitySDK_INSTALL_DIR}/build/native" )
        set( DX12AgilitySDK_INCLUDE_DIR "${DX12AgilitySDK_DIR}/include" )
        message( WARNING "Unable to find DXAgilitySDK at ${RpsDXAgilitySDK_DIR}. Falling back to ${DXAgilitySDK_INSTALL_DIR}" )
    else()
        set( DX12AgilitySDK_DIR "${RpsDXAgilitySDK_DIR}/build/native" )
    endif()

    # acquire agility SDK available if not available.
    if( NOT EXISTS "${DX12AgilitySDK_INCLUDE_DIR}" )
        message( STATUS "Unable to find DX12AgilitySDK_INCLUDE_DIR at ${DX12AgilitySDK_INCLUDE_DIR}" )
        message( STATUS "Attempting to install DXAgilitySDK to ${DXAgilitySDK_INSTALL_DIR}" )

        # check internet connection.
        if( WIN32 )
            set ( PingCountArg "-n" )
        else()
            set ( PingCountArg "-c" )
        endif()
        execute_process(
            COMMAND ping www.amd.com ${PingCountArg} 2 -w 1000
            ERROR_QUIET
            OUTPUT_QUIET
            RESULT_VARIABLE PING_STATUS
        )

        # download agility sdk.
        if ( PING_STATUS GREATER 0 )
            set ( RpsEnableDXAgilitySDK OFF )
            message( WARNING "Cannot download DXAgilitySDK as no internet connection. Unsetting RpsEnableDXAgilitySDK." )
        else()
            set( DXAgilitySDK_DOWNLOAD_URL "https://www.nuget.org/api/v2/package/Microsoft.Direct3D.D3D12/${DXAgilitySDK_VERSION_STRING}" )
            message( STATUS "Downloading DXAgilitySDK from ${DXAgilitySDK_DOWNLOAD_URL} ..." )
            file( MAKE_DIRECTORY "${DXAgilitySDK_INSTALL_DIR}" )
            file( DOWNLOAD "${DXAgilitySDK_DOWNLOAD_URL}" "${DXAgilitySDK_INSTALL_DIR}.zip" )
            message( VERBOSE "Extracting ${DXAgilitySDK_INSTALL_DIR}.zip to ${DXAgilitySDK_INSTALL_DIR} ..." )
            execute_process(
                COMMAND ${CMAKE_COMMAND} "-E" "tar" "xvz" "${DXAgilitySDK_INSTALL_DIR}.zip"
                WORKING_DIRECTORY "${DXAgilitySDK_INSTALL_DIR}"
                OUTPUT_QUIET
                ERROR_VARIABLE DX12AgilitySDK_ERROR
            )

            message( VERBOSE "Cleaning ${DXAgilitySDK_INSTALL_DIR}.zip ..." )
            # cleanup temp folder
            file( REMOVE_RECURSE "${DXAgilitySDK_INSTALL_DIR}.zip" )
            
            if ( "${DX12AgilitySDK_ERROR}" STREQUAL "" )
                message( STATUS "Successfully installed DXAgilitySDK to ${DXAgilitySDK_INSTALL_DIR}" )
                message( STATUS "DX12AgilitySDK_INCLUDE_DIR = ${DX12AgilitySDK_INCLUDE_DIR}")
            else()
                set ( RpsEnableDXAgilitySDK OFF )
                message( WARNING "Unsetting RpsEnableDXAgilitySDK. DXAgilitySDK install failed with: ${DX12AgilitySDK_ERROR}" )
            endif()
        endif()
    else()
        message( STATUS "DX12AgilitySDK_INCLUDE_DIR = ${DX12AgilitySDK_INCLUDE_DIR}" )
    endif()

endif( )

set_property( GLOBAL PROPERTY USE_FOLDERS ON )
if ( MSVC )
    set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /UMBCS /WX" )

    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-private-field" )
    endif()
elseif( CMAKE_CXX_COMPILER_ID MATCHES "Clang" )
    set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -Wno-unused-variable -Wno-unused-private-field" )
endif ( )

if (WIN32 AND "$ENV{CXXFLAGS}" MATCHES "-fsanitize=address" )
    message( STATUS "ASAN enabled. Disabling SEH" )
    add_definitions( -DCATCH_CONFIG_NO_WINDOWS_SEH )
endif( )

if ( RpsEnableImGui AND (CMAKE_SIZEOF_VOID_P LESS 8) )
    add_definitions( -DImTextureID=ImU64 )
endif( )

include(CheckIncludeFiles)

function( CheckIncludeFilesAndAddDefinition IncludeFileName DefinitionName )
    check_include_files( ${IncludeFileName} IncludeFileFound)
    if ( ${IncludeFileFound} )
        add_definitions( -D${DefinitionName} )
    endif( )
endfunction( )

CheckIncludeFilesAndAddDefinition(intrin.h RPS_HAS_INTRIN_H)

function( TryCompileFileAndAddDefinition TryCompileFileName DefinitionName )
    try_compile( TryCompileSucceeded ${CMAKE_CURRENT_BINARY_DIR}/cmake_try_compile ${CMAKE_CURRENT_SOURCE_DIR}/tools/cmake_tests/${TryCompileFileName} )
    message( STATUS ${DefinitionName} = ${TryCompileSucceeded} )
    if ( ${TryCompileSucceeded} )
        add_definitions( -D${DefinitionName} )
    endif()
endfunction( )

TryCompileFileAndAddDefinition( has_bitscan.c RPS_HAS_BITSCAN )
TryCompileFileAndAddDefinition( has_builtin_clz_ctz.c RPS_HAS_BUILTIN_CLZ_CTZ )
TryCompileFileAndAddDefinition( has_nodiscard.cpp RPS_HAS_NODISCARD )
TryCompileFileAndAddDefinition( has_maybe_unused.cpp RPS_HAS_MAYBE_UNUSED )
TryCompileFileAndAddDefinition( has_popcnt.c RPS_HAS_POPCNT )
TryCompileFileAndAddDefinition( has_builtin_popcount.c RPS_HAS_BUILTIN_POPCOUNT )

if ( RpsBuildTests )
    enable_testing()
endif ( )

# Compile RPSL
function( CompileRpslDxc TargetName RpslFileName GeneratedSources OutDirPrefix )
    get_filename_component( FileNameWithoutExtension ${RpslFileName} NAME_WE )
    set( OutDirectory "${CMAKE_CURRENT_BINARY_DIR}/${TargetName}/${OutDirPrefix}/" )
    # message( STATUS "Rps-hlslc Output Dir: " ${OutDirectory} )
    # message( STATUS "Working dir: " ${OutDirectory} )
    set( OutputSource ${OutDirectory}${FileNameWithoutExtension}.rpsl.g.c )
    string( APPEND RpsCompileOpts
        "$<IF:$<CONFIG:DEBUG>,-O0,-O3>")
    add_custom_command(
        OUTPUT ${OutputSource}
        COMMAND "${PROJECT_SOURCE_DIR}/tools/rps_hlslc/bin/rps-hlslc.exe" "${RpslFileName}" -od "${OutDirectory}" -m ${FileNameWithoutExtension} ${RpsCompileOpts}
        COMMAND ${CMAKE_COMMAND} -E echo "Compiling RPSL ${RpslFileName} : '${PROJECT_SOURCE_DIR}/tools/rps_hlslc/bin/rps-hlslc.exe ${RpslFileName} -od ${OutDirectory} -m ${FileNameWithoutExtension} ${RpsCompileOpts}'"
        WORKING_DIRECTORY ${OutDirectory}
        DEPENDS ${RpslFileName} "${PROJECT_SOURCE_DIR}/tools/rps_hlslc/bin/rps-hlslc.exe" "${PROJECT_SOURCE_DIR}/tools/rps_hlslc/bin/dxcompiler.dll" "${PROJECT_SOURCE_DIR}/tools/rps_hlslc/bin/llvm-cbe.exe"
        VERBATIM )
    set( GeneratedSources "${GeneratedSources}" "${OutputSource}" PARENT_SCOPE )
endfunction()

# Copy assets
function( CopyShaders TargetName ShaderFiles SrcFolder )
    foreach( Shader ${ShaderFiles} )
        message( STATUS "Copy Shader: ${Shader} => $<TARGET_FILE_DIR:${TargetName}>" )
        add_custom_command( TARGET ${TargetName} PRE_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${Shader}" "$<TARGET_FILE_DIR:${TargetName}>" )
    endforeach()
endfunction()

function( CopyDXC TargetName )
    #x64 only for now
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        add_custom_command(
            TARGET ${TargetName} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${PROJECT_SOURCE_DIR}/external/dxc/x64/dxil.dll
            $<TARGET_FILE_DIR:${TargetName}>)
        add_custom_command(
            TARGET ${TargetName} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${PROJECT_SOURCE_DIR}/tools/rps_hlslc/bin/dxcompiler.dll
            $<TARGET_FILE_DIR:${TargetName}>)
    endif()
endfunction()

function( CopyJITCompiler TargetName )
    #x64 only for now
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        add_custom_command(
            TARGET ${TargetName} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${PROJECT_SOURCE_DIR}/tools/rps_hlslc/bin/rps-jit.dll
            $<TARGET_FILE_DIR:${TargetName}>)
        # TODO: For now we need rps-hlslc.exe to compile rpsl to bitcode.
        # Should allow dxcompiler.dll handle this.
        add_custom_command(
            TARGET ${TargetName} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${PROJECT_SOURCE_DIR}/tools/rps_hlslc/bin/rps-hlslc.exe
            $<TARGET_FILE_DIR:${TargetName}>)
        CopyDXC( ${TargetName} )
    endif()
endfunction()

function( CopyDX12AgilitySDKBinaries TargetName )
    if (RpsEnableDXAgilitySDK)
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            set( AgilitySDKPlatformName x64 )
        endif()
        foreach(BinaryToCopy D3D12Core.dll D3D12SDKLayers.dll)
            add_custom_command(
                TARGET ${TargetName} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${DX12AgilitySDK_DIR}/bin/${AgilitySDKPlatformName}/${BinaryToCopy}
                $<TARGET_FILE_DIR:${TargetName}>/D3D12/${BinaryToCopy} )
        endforeach()
    endif()
endfunction()

# Adding a library module from the ./src/${ModuleName} folder
function( AddModule ModuleName ModuleFolder SrcFolder SrcInclude CompileFlags LibType LinkLibs )
    file( GLOB_RECURSE HeaderFiles
        "${ModuleFolder}/*.h"
        "${ModuleFolder}/*.hpp"
        "${ModuleFolder}/*.inl" )
    file( GLOB_RECURSE SourceFiles
        "${ModuleFolder}/*.c"
        "${ModuleFolder}/*.cpp" )
    source_group( TREE "${ModuleFolder}/" FILES ${HeaderFiles} ${SourceFiles} )

    if ( SourceFiles )
        add_library( ${ModuleName} ${LibType} ${HeaderFiles} ${SourceFiles} )
        target_include_directories( ${ModuleName} PRIVATE ${PROJECT_SOURCE_DIR}/include/ ${PROJECT_SOURCE_DIR}/src/ ${SrcInclude} )
    else ( )
        set(ModuleName ${ModuleName}.headers)
        add_custom_target( ${ModuleName} SOURCES ${HeaderFiles} )
    endif ( )

    if ( CompileFlags )
        set_target_properties( ${ModuleName} PROPERTIES COMPILE_FLAGS "${CompileFlags}")
    endif ( )

    if ( SrcFolder )
        set_target_properties( ${ModuleName} PROPERTIES FOLDER "${SrcFolder}")
    endif ( )

    if ( LinkLibs )
        target_link_libraries( ${ModuleName} PRIVATE ${LinkLibs} )
    endif ( )

endfunction( )

# Adding a library module from the ./src/${ModuleName} folder without recursion
function( AddModuleNoRecurse ModuleName ModuleFolder SrcFolder SrcInclude CompileFlags LibType )
    file( GLOB HeaderFiles
        "${ModuleFolder}/*.h"
        "${ModuleFolder}/*.hpp"
        "${ModuleFolder}/*.inl" )
    file( GLOB SourceFiles
        "${ModuleFolder}/*.c"
        "${ModuleFolder}/*.cpp" )
    source_group( TREE "${ModuleFolder}/" FILES ${HeaderFiles} ${SourceFiles} )

    if ( SourceFiles )
        add_library( ${ModuleName} ${LibType} ${HeaderFiles} ${SourceFiles} )
        target_include_directories( ${ModuleName} PRIVATE ${PROJECT_SOURCE_DIR}/include/ ${PROJECT_SOURCE_DIR}/src/ ${SrcInclude} )
    else ( )
        add_library( ${ModuleName} INTERFACE )
        add_custom_target( ${ModuleName}.headers SOURCES ${HeaderFiles} )
        target_include_directories( ${ModuleName} INTERFACE ${PROJECT_SOURCE_DIR}/src/ )
    endif ( )

    if ( CompileFlags )
        set_target_properties( ${ModuleName} PROPERTIES COMPILE_FLAGS "${CompileFlags}")
    endif ( )

    if ( SrcFolder )
        set_target_properties( ${ModuleName} PROPERTIES FOLDER "${SrcFolder}")
    endif ( )
endfunction( )

if(${CMAKE_VERSION} VERSION_LESS "3.16.0") 
    message( WARNING "Current CMake version doesn't support VS_DPI_AWARE. Consider update to 3.16.0 or newer")
endif()

# Adding a sample project
function( AddCppApp AppName AppFolder SrcFolder SrcSharedFolder SrcInclude CompileFlags DependenciesString )
    file( GLOB_RECURSE SourceFiles
        "${SrcFolder}/*.h"
        "${SrcFolder}/*.hpp"
        "${SrcFolder}/*.inl"
        "${SrcFolder}/*.c"
        "${SrcFolder}/*.cpp" )
    source_group( TREE "${SrcFolder}/" FILES ${SourceFiles} )

    file (GLOB_RECURSE ShaderFiles
        "${SrcFolder}/*.hlsl"
        "${SrcFolder}/*.glsl"
        "${SrcFolder}/*.dxbc"
        "${SrcFolder}/*.spv"
        "${SrcFolder}/*.frag"
        "${SrcFolder}/*.vert"
        "${SrcFolder}/*.spv" )
    source_group( TREE "${SrcFolder}/" PREFIX "shaders" FILES ${ShaderFiles} )

    if ( SrcSharedFolder )
        file( GLOB_RECURSE SourceSharedFiles
            "${SrcSharedFolder}/*.h" )
        source_group( TREE "${SrcSharedFolder}/" PREFIX "shared" FILES ${SourceSharedFiles} )

        if ( ${AppName} MATCHES "rpsl" )
            file( GLOB_RECURSE RpslFiles
                "${SrcSharedFolder}/*.rpsl" )
            source_group( TREE "${SrcSharedFolder}/" PREFIX "rpsl" FILES ${RpslFiles} )
        endif( )

        set( GeneratedSources "" )
        foreach( RpslFileName ${RpslFiles} )
            message( STATUS "Found Rpsl: " ${RpslFileName} )
            CompileRpslDxc( ${AppName} ${RpslFileName} "${GeneratedSources}" "Generated")
        endforeach()
        source_group( TREE "${CMAKE_CURRENT_BINARY_DIR}/${AppName}/Generated/" PREFIX "rps_generated" FILES ${GeneratedSources} )

    endif( )

    set_source_files_properties( ${ShaderFiles} PROPERTIES VS_COPY_TO_OUT_DIR "Always" VS_TOOL_OVERRIDE "Content" )

    add_executable( ${AppName} WIN32 ${SourceFiles} ${ShaderFiles} ${RpslFiles} ${GeneratedSources} ${SourceSharedFiles} )

    CopyShaders( ${AppName} "${ShaderFiles}" "${SrcFolder}" )

    target_include_directories( ${AppName} PRIVATE
        ${PROJECT_SOURCE_DIR}/include/
        ${PROJECT_SOURCE_DIR}/src/
        ${SrcFolder}/
        ${SrcInclude}
    )
    set( DependencyList ${DependenciesString} )
    target_link_libraries( ${AppName} ${DependencyList} )

    if ( ${CompileFlags} MATCHES "CONSOLE" )
        message(${AppName})
        set( LinkFlags /SUBSYSTEM:CONSOLE )
    endif( )

    set_target_properties( ${AppName} PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${SrcFolder}"
        COMPILE_FLAGS "${CompileFlags}"
        FOLDER "${AppFolder}"
        LINK_FLAGS "${LinkFlags}"
        VS_DPI_AWARE "PerMonitor"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${AppName}")

endfunction()

function( AddSampleApps AppNames Variant Platform SrcInclude CompileFlags DependenciesString )
    foreach(AppName ${AppNames})
        AddCppApp( ${AppName}${Variant}_${Platform} "samples/${Platform}" ${PROJECT_SOURCE_DIR}/samples/${Platform}/${AppName} ${PROJECT_SOURCE_DIR}/samples/shared/${AppName} "${SrcInclude}" ${CompileFlags} "${DependenciesString}" )
    endforeach()
endfunction()

set ( FullSource TRUE )


# Common for full or limited source
BuildFolderProperty( "modules" ModuleFolder )
AddModule( rps ${PROJECT_SOURCE_DIR}/include "${ModuleFolder}" "" "" INTERFACE "" )

add_subdirectory( src )
add_subdirectory( external )

if ( RpsBuildTests )
    add_subdirectory( "${CMAKE_CURRENT_SOURCE_DIR}/tests" )
endif ( )

if ( RpsBuildTools )
    add_subdirectory( tools )
endif ( )
