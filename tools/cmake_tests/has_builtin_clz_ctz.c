// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

int main()
{
    unsigned int a = 0x42;
    unsigned int b = __builtin_clz(a);
    unsigned int c = __builtin_ctz(b);
    unsigned int d = __builtin_clzll(a);
    unsigned int e = __builtin_ctzll(b);

    return (int)(a + b + c + d + e);
}