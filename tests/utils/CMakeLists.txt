# Copyright (c) 2022 Advanced Micro Devices, Inc. All rights reserved.
#
# This file is part of the AMD Render Pipeline Shaders SDK which is
# released under the AMD INTERNAL EVALUATION LICENSE.
#
# See file LICENSE.RTF for full license details.

file( GLOB_RECURSE UtilHeaders
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp" )


AddModule( rps_test_utils ${CMAKE_CURRENT_SOURCE_DIR} "${AppFolder}" "" "" INTERFACE "" )