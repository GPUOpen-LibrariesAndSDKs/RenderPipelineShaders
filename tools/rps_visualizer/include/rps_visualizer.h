// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_VISUALIZER_H
#define RPS_VISUALIZER_H

#include "rps/runtime/common/rps_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif  //__cplusplus

/// @brief Bitflags for properties of the visualizer.
typedef enum RpsVisualizerCreateFlagBits
{
    /// No special properties.
    RPS_VISUALIZER_CREATE_NONE = 0,

    /// The visualizer is used as a child window of a user controlled imgui window. Calling ImGui::BeginChild and
    /// ImGui::EndChild is still required for the user.
    RPS_VISUALIZER_CREATE_CHILD_WINDOW_BIT = 1 << 0,
} RpsVisualizerCreateFlagBits;

/// @brief Bitmask type for <c><i>RpsVisualizerCreateFlagBits</i></c>.
typedef RpsFlags32 RpsVisualizerCreateFlags;

/// @brief Parameters for creating a visualizer.
typedef struct RpsVisualizerCreateInfo
{
    RpsVisualizerCreateFlags flags;               ///< Creation flags.
    const char*              settingsFolderPath;  ///< Path to the folder in which a configuration file may be located
                                                  ///  for reading and writing persistent state.
} RpsVisualizerCreateInfo;

/// @brief Parameters for creating a visualizer.
typedef struct RpsVisualizerUpdateInfo
{
    RpsRenderGraph hRenderGraph;  ///< Handle to the render graph to visualize data for.
} RpsVisualizerUpdateInfo;

/// @brief Handle type for visualizer objects.
RPS_DEFINE_HANDLE(RpsVisualizer);

/// @brief Creates a visualizer.
///
/// @param hDevice                  Handle to the device to use for creation.
/// @param pCreateInfo              Pointer to the creation parameters. Passing NULL uses default parameters instead.
/// @param phVisualizer             Pointer to a handle in which the visualizer is returned. Must not be NULL.
///
/// @returns                        Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsVisualizerCreate(RpsDevice                      hDevice,
                              const RpsVisualizerCreateInfo* pCreateInfo,
                              RpsVisualizer*                 phVisualizer);

/// @brief Destroys a visualizer.
///
/// @param hVisualizer              Handle to the visualizer to destroy.
///
/// @returns                        Result code of the operation. See <c><i>RpsResult</i></c> for more info.
void rpsVisualizerDestroy(RpsVisualizer hVisualizer);

/// @brief Updates a visualizer to the latest state of a render graph.
///
/// To always show the latest version of the render graph in use the visualizer should be updated after each render
/// graph update that may have caused changes w.r.t. heap placements and sizes, number and type of commands, number,
/// type and size of resources, scheduling order, etc. This does not have to be every frame but e.g. after window
/// resizes, changed constant parameters of the render graph, etc.
///
/// @param hVisualizer              Handle to the visualizer to update.
/// @param pUpdateInfo              Pointer to the update parameters. Must not be NULL.
///
/// @returns                        Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsVisualizerUpdate(RpsVisualizer hVisualizer, const RpsVisualizerUpdateInfo* pUpdateInfo);

/// @brief Draws the visualizer UI with ImGui.
///
/// @param hVisualizer              Handle to the visualizer to draw the overlay for.
///
/// @returns                        Result code of the operation. See <c><i>RpsResult</i></c> for more info.
RpsResult rpsVisualizerDrawImGui(RpsVisualizer hVisualizer);

#ifdef __cplusplus
}
#endif  //__cplusplus

#endif  //RPS_VISUALIZER_H
