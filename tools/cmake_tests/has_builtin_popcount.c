// Copyright (c) 2022 Advanced Micro Devices, Inc. All rights reserved.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the AMD INTERNAL EVALUATION LICENSE.
//
// See file LICENSE.RTF for full license details.

int main()
{
    unsigned int a = 0x42u;
    unsigned int b = __builtin_popcount(a);
    return (a + b);
}