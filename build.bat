@REM Copyright (c) 2024 Advanced Micro Devices
@REM
@REM This file is part of the AMD Render Pipeline Shaders SDK which is
@REM released under the MIT LICENSE.
@REM
@REM See file LICENSE.txt for full license details.

@REM If you are committing changes in the submodules of this project, you may want
@REM to comment out this line to avoid having them reset to their intended commit.
git submodule update --init

cmake -S ./ -B ./build -A x64
cmake --build ./build --config RelWithDebInfo

