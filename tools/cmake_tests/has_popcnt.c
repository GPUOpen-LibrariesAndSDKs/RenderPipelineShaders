// Copyright (c) 2023 Advanced Micro Devices, Inc. All rights reserved.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the AMD INTERNAL EVALUATION LICENSE.
//
// See file LICENSE.txt for full license details.

#include <intrin.h>

int main()
{
    unsigned int a = 0x42u;
    unsigned int b = __popcnt(a);
    return (a + b);
}