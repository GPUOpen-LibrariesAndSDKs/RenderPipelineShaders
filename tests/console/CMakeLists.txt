# Copyright (c) 2024 Advanced Micro Devices, Inc.
#
# This file is part of the AMD Render Pipeline Shaders SDK which is
# released under the MIT LICENSE.
#
# See file LICENSE.txt for full license details.


set( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE} )

file( GLOB_RECURSE ConsoleTestSrcs
    "${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp" )

message( STATUS "Found Tests: ${ConsoleTestSrcs}" )

if ( NOT RpsJITSupported )
    list(APPEND ExcludeTests "test_rpsl_jit")
endif()

foreach( TestSrc ${ConsoleTestSrcs} )
    SetupTestApp( ${TestSrc} "${AppFolder}/console" "" "" True False)
endforeach( )

# Per app settings
if ( RpsJITSupported )
    CopyJITCompiler(test_rpsl_jit)
endif()
