// Copyright (c) 2023 Advanced Micro Devices, Inc. All rights reserved.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the AMD INTERNAL EVALUATION LICENSE.
//
// See file LICENSE.txt for full license details.

[[maybe_unused]] int foo(bool b)
{
    [[maybe_unused]] bool b1 = b;
    return 1;
}

int main()
{
    return 0;
}
