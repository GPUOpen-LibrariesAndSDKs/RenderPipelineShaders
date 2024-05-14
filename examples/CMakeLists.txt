# Copyright (c) 2024 Advanced Micro Devices, Inc.
#
# This file is part of the AMD Render Pipeline Shaders SDK which is
# released under the MIT LICENSE.
#
# See file LICENSE.txt for full license details.

BuildFolderProperty( "examples" AppFolder )

if ( WIN32 )

    file( GLOB_RECURSE ExampleSrcs
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp" )

    set( LibDependencies "rps_runtime_d3d12" )

    foreach( ExampleSrc ${ExampleSrcs} )

        get_filename_component( TargetName ${ExampleSrc} NAME_WE )
        set( GeneratedSources "" )
        set( RpslFileName ${CMAKE_CURRENT_SOURCE_DIR}/${TargetName}.rpsl )

        CompileRpslDxc( ${TargetName} ${RpslFileName} GeneratedSources "Generated" )

        # source_group is providing a folder in IDE environment for generated files.
        source_group( TREE "${CMAKE_CURRENT_BINARY_DIR}/${TargetName}.tmp/Generated/" PREFIX "rps_generated" FILES ${GeneratedSources} )

        add_executable( ${TargetName} WIN32 ${ExampleSrc} ${GeneratedSources} ${RpslFileName} )

        CopyDXC( ${TargetName} )
        CopyDX12AgilitySDKBinaries( ${TargetName} )

        target_include_directories( ${TargetName} PRIVATE
            ${PROJECT_SOURCE_DIR}/tools/
            ${PROJECT_SOURCE_DIR}/include/
        )

        target_link_libraries( ${TargetName} ${LibDependencies} )

        if ( ${CompileFlags} MATCHES "CONSOLE" )
            set( LinkFlags /SUBSYSTEM:CONSOLE )
        endif( )

        set_target_properties( ${TargetName} PROPERTIES
            CXX_STANDARD 17
            COMPILE_FLAGS "${CompileFlags}"
            FOLDER "${AppFolder}"
            LINK_FLAGS "${LinkFlags}"
            VS_DPI_AWARE "PerMonitor" )

    endforeach( )

endif()
