# Copyright (c) 2023 Advanced Micro Devices, Inc. All rights reserved.
#
# This file is part of the AMD Render Pipeline Shaders SDK which is
# released under the AMD INTERNAL EVALUATION LICENSE.
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
    SetupTestApp( ${TestSrc} "${AppFolder}/console" "" "" False False)
endforeach( )

# Per app settings
if ( RpsJITSupported )
    CopyJITCompiler(test_rpsl_jit)
endif()
