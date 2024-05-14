// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#include <intrin.h>

int main()
{
    unsigned int a = 0x42u;
    unsigned int b = __popcnt(a);
    return (a + b);
}