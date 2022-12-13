@REM Copyright (c) 2022 Advanced Micro Devices, Inc. All rights reserved.
@REM
@REM This file is part of the AMD Render Pipeline Shaders SDK which is
@REM released under the AMD INTERNAL EVALUATION LICENSE.
@REM
@REM See file LICENSE.RTF for full license details.

cmake -S ./ -B ./build -A x64
cmake --build ./build --config RelWithDebInfo

