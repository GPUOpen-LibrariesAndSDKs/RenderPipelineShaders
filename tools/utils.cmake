# Copyright (c) 2024 Advanced Micro Devices, Inc.
#
# This file is part of the AMD Render Pipeline Shaders SDK which is
# released under the MIT LICENSE.
#
# See file LICENSE.txt for full license details.

function( BuildFolderProperty RelativeFolder OutputVar )
    if ( "${RpsRootSolutionFolder}" STREQUAL "" )
        set( ${OutputVar} "${RelativeFolder}" PARENT_SCOPE )
    else()
        set( ${OutputVar} "${RpsRootSolutionFolder}/${RelativeFolder}" PARENT_SCOPE )
    endif()
endfunction()

function( CheckFindFile VarName VarValue)
    if ( "${VarValue}" MATCHES "NOTFOUND" )
        message( WARNING "Unable to locate ${VarName}" )
    else()
        message( STATUS "Found ${VarName}=${VarValue}" )
    endif()
endfunction()

function( FindSpirvTools DXCOMPILER_DLL_VAR )
    find_file( DXCOMPILER_DLL "dxcompiler.dll" PATHS ${Vulkan_INCLUDE_DIRS}/../Bin NO_DEFAULT_PATH NO_CACHE )
    set( ${DXCOMPILER_DLL_VAR} ${DXCOMPILER_DLL} PARENT_SCOPE )
endfunction()

if ( WIN32 )

    function( FindWin10SdkRoot Win10SdkRootVar )
        message( STATUS $ENV{ProgramFiles} )
        get_filename_component( Win10RegKey "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots;KitsRoot10]" ABSOLUTE )
        set( PotentialRoots "${Win10RegKey}" "$ENV{ProgramFiles\(x86\)}/Windows Kits/10/" "$ENV{ProgramFiles}/Windows Kits/10/" )
        foreach(Root ${PotentialRoots})
            file( GLOB Win10SdkVersionDirs "${Root}/Include/10.0.*.*" )
            if ( Win10SdkVersionDirs )
                set( ${Win10SdkRootVar} ${Root} PARENT_SCOPE )
                return()
            endif()
        endforeach()
    endfunction()

    function( FindDXC DXCOMPILER_DLL_VAR DXIL_DLL_VAR )

        if ( NOT "${RpsDXC_DIR}" STREQUAL "" )
            find_file( DXCOMPILER_DLL "dxcompiler.dll" PATHS "${RpsDXC_DIR}" NO_DEFAULT_PATH NO_CACHE )
            find_file( DXIL_DLL "dxil.dll" PATHS "${RpsDXC_DIR}" NO_DEFAULT_PATH NO_CACHE )
            if ((EXISTS "${DXCOMPILER_DLL}") AND (EXISTS "${DXIL_DLL}"))
                set( Root_DLL_DIR "${RpsDXC_DIR}" )
            else()
                message( WARNING "Cannot find either dxcompiler.dll or dxil.dll in RpsDXC_DIR=${RpsDXC_DIR}. Falling back to DXC bundled with Windows SDK." )
            endif()
        endif()
        
        if ( NOT Root_DLL_DIR )
            FindWin10SdkRoot( Win10SdkRoot )
            if ( NOT CMAKE_WINDOWS_KITS_10_DIR )
                set( CMAKE_WINDOWS_KITS_10_DIR ${Win10SdkRoot} CACHE PATH "The specified directory is expected to contain Include/10.0.* directories.")
            endif()

            file( GLOB Win10SdkVersionDirs "${CMAKE_WINDOWS_KITS_10_DIR}/Include/10.0.*.*" )

            if ( NOT Win10SdkVersionDirs )
                message( WARNING "CMAKE_WINDOWS_KITS_10_DIR is invalid, CMAKE_WINDOWS_KITS_10_DIR=${CMAKE_WINDOWS_KITS_10_DIR}" )
            endif()

            if ( CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION )
                STRING( REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+" Win10SdkVersionString ${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION} )
            else()
                list( SORT Win10SdkVersionDirs )
                list( REVERSE Win10SdkVersionDirs )
                list( GET Win10SdkVersionDirs 0 Win10SdkVersionDirLargest )
                STRING( REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+" Win10SdkVersionString ${Win10SdkVersionDirLargest} )
            endif()

            if ( RPS_X64 )
                set( Root_DLL_DIR "$CACHE{CMAKE_WINDOWS_KITS_10_DIR}/bin/${Win10SdkVersionString}/x64" )
            elseif ( RPS_X86 )
                set( Root_DLL_DIR "$CACHE{CMAKE_WINDOWS_KITS_10_DIR}/bin/${Win10SdkVersionString}/x86" )
            endif()

            find_file( DXCOMPILER_DLL "dxcompiler.dll" PATHS "${Root_DLL_DIR}" NO_DEFAULT_PATH NO_CACHE )
            find_file( DXIL_DLL "dxil.dll" PATHS "${Root_DLL_DIR}" NO_DEFAULT_PATH NO_CACHE )

        endif() 

        set( ${DXCOMPILER_DLL_VAR} ${DXCOMPILER_DLL} PARENT_SCOPE )
        set( ${DXIL_DLL_VAR} ${DXIL_DLL} PARENT_SCOPE )

    endfunction()
endif()