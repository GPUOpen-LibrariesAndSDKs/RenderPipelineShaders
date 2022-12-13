# Copyright (c) 2022 Advanced Micro Devices, Inc. All rights reserved.
#
# This file is part of the AMD Render Pipeline Shaders SDK which is
# released under the AMD INTERNAL EVALUATION LICENSE.
#
# See file LICENSE.RTF for full license details.

BuildFolderProperty( "tools" AppFolder )

if ( ${RpsEnableImGui} )
    add_subdirectory(rpsl_explorer)
    add_subdirectory(rps_visualizer)
endif()
