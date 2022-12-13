// Copyright (c) 2022 Advanced Micro Devices, Inc. All rights reserved.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the AMD INTERNAL EVALUATION LICENSE.
//
// See file LICENSE.RTF for full license details.

#ifndef _RPS_TEST_D3D12_RENDERER_H_
#define _RPS_TEST_D3D12_RENDERER_H_

#include "app_framework/afx_d3d12_renderer.h"

#ifndef TEST_APP_NAME
#define TEST_APP_NAME (TEST_APP_NAME_RAW " - D3D12")
#endif

class RpsTestD3D12Renderer : public RpsAfxD3D12Renderer
{
};

#endif  //_RPS_TEST_D3D12_RENDERER_H_