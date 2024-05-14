// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_VISUALIZER_UTIL_HPP
#define RPS_VISUALIZER_UTIL_HPP

#include <cstdint>
#include <cmath>
#include <string>
#include <inttypes.h>

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"

#include "core/rps_util.hpp"
#include "rps/runtime/common/rps_access.h"

namespace rps
{

    struct UIRect
    {
        ImVec2 start;
        ImVec2 end;

        inline bool IsEmpty() const
        {
            return start.x == end.x || start.y == end.y;
        }
        inline bool Contains(ImVec2 pos) const
        {
            return start.x <= pos.x && pos.x <= end.x && start.y <= pos.y && pos.y <= end.y;
        }
        ImVec2 GetSize() const
        {
            return end - start;
        }
    };

    struct U64Vec2
    {
        uint64_t x;
        uint64_t y;
    };

    static inline std::pair<uint64_t, const char*> CalcMemoryFormatUnit(uint64_t desiredResolutionInBytes)
    {
        static constexpr const char* memUnits[] = {
            "B",
            "KiB",
            "MiB",
            "GiB",
            "TiB",
            "PiB",
            "EiB",
        };

        const uint32_t unitIdx =
            desiredResolutionInBytes ? (64 - rpsFirstBitHigh(desiredResolutionInBytes) - 1) / 10 : 0;

        return std::make_pair((uint64_t(1) << (unitIdx * 10)), memUnits[unitIdx]);
    }

    static inline std::pair<float, const char*> FormatMemorySize(uint64_t memorySize)
    {
        const auto  unit  = CalcMemoryFormatUnit(memorySize);
        const float value = float(double(memorySize) / unit.first);

        return std::make_pair(value, unit.second);
    }

    static inline void FormatMemorySize(PrinterRef printer, uint64_t memorySize, uint64_t desiredResolution)
    {
        const auto unit = CalcMemoryFormatUnit(desiredResolution);

        if (unit.first == 1)
        {
            printer("%" PRIu64 " %s", memorySize, unit.second);
        }
        else
        {
            printer("%" PRIu64 " %s", uint64_t(round(double(memorySize) / unit.first)), unit.second);
        }
    }

    static constexpr double GetNextDecimalHalfBaseLimited(double input)
    {
        assert(input >= 0);

        for (double base = 10.; base < 1000000.; base *= 10)
        {
            if (input < base)
            {
                const double halfBase = base / 2;
                if (input < halfBase)
                {
                    return base / 10.;
                }
                return halfBase;
            }
        }
        //Should never happen
        return 1000000.;
    }

    static inline double GetPrevBinaryBase(double input)
    {
        double log = std::floor(std::log2(input));

        return static_cast<double>(1 << static_cast<uint64_t>(log));
    }

    static inline ImU32 LerpHSV(float interpolant, float value = 1.f, float alpha = 1.f)
    {
        //Always use maximum saturation S and value V
        const float S = 1.f;
        const float V = value;
        const float C = S * V;
        const float A = alpha;

        const float HPrime = interpolant * 360 / 60;
        const float X      = C * (1 - std::abs(std::fmod(HPrime, 2.0f) - 1));

        if (HPrime <= 1.f)
        {
            return ImGui::ColorConvertFloat4ToU32({C, X, .0f, A});
        }
        else if (HPrime <= 2.f)
        {
            return ImGui::ColorConvertFloat4ToU32({X, C, .0f, A});
        }
        else if (HPrime <= 3.f)
        {
            return ImGui::ColorConvertFloat4ToU32({.0f, C, X, A});
        }
        else if (HPrime <= 4.f)
        {
            return ImGui::ColorConvertFloat4ToU32({.0f, X, C, A});
        }
        else if (HPrime <= 5.f)
        {
            return ImGui::ColorConvertFloat4ToU32({X, .0f, C, A});
        }
        else if (HPrime <= 6.f)
        {
            return ImGui::ColorConvertFloat4ToU32({C, .0f, X, A});
        }
        else
        {
            return ImGui::ColorConvertFloat4ToU32({.0f, .0f, .0f, 1.f});
        }
    }

    enum class ResourceAccessCategory
    {
        RenderTarget = 0,
        DepthStencilWrite,
        DepthStencilRead,
        UnorderedAccessWrite,
        CopyResolveWrite,
        CopyResolveRead,
        ShaderResourceRead,
        NonShaderResourceRead,
        RaytracingASWrite,
        RaytracingASRead,
        OtherWrite,
        OtherRead,
        Other,
        Count,
    };

    static inline ResourceAccessCategory GetAccessCategoryFromAccessFlags(RpsAccessFlags accessFlags)
    {
        if (accessFlags & RPS_ACCESS_RENDER_TARGET_BIT)
        {
            return ResourceAccessCategory::RenderTarget;
        }

        if (accessFlags & RPS_ACCESS_DEPTH_STENCIL_WRITE)
        {
            return ResourceAccessCategory::DepthStencilWrite;
        }

        if (accessFlags & RPS_ACCESS_UNORDERED_ACCESS_BIT)
        {
            return ResourceAccessCategory::UnorderedAccessWrite;
        }

        if (accessFlags & (RPS_ACCESS_RESOLVE_DEST_BIT | RPS_ACCESS_COPY_DEST_BIT))
        {
            return ResourceAccessCategory::CopyResolveWrite;
        }

        if (accessFlags & RPS_ACCESS_RAYTRACING_AS_BUILD_BIT)
        {
            return ResourceAccessCategory::RaytracingASWrite;
        }

        if (accessFlags & RPS_ACCESS_DEPTH_STENCIL_READ)
        {
            return ResourceAccessCategory::DepthStencilRead;
        }

        if (accessFlags & (RPS_ACCESS_RESOLVE_SRC_BIT | RPS_ACCESS_COPY_SRC_BIT))
        {
            return ResourceAccessCategory::CopyResolveRead;
        }

        if (accessFlags & RPS_ACCESS_SHADER_RESOURCE_BIT)
        {
            return ResourceAccessCategory::ShaderResourceRead;
        }

        if (accessFlags & (RPS_ACCESS_CONSTANT_BUFFER_BIT | RPS_ACCESS_VERTEX_BUFFER_BIT | RPS_ACCESS_INDEX_BUFFER_BIT |
                           RPS_ACCESS_INDIRECT_ARGS_BIT | RPS_ACCESS_SHADING_RATE_BIT))
        {
            return ResourceAccessCategory::NonShaderResourceRead;
        }

        if (accessFlags & RPS_ACCESS_RAYTRACING_AS_READ_BIT)
        {
            return ResourceAccessCategory::RaytracingASRead;
        }

        if (accessFlags & (RPS_ACCESS_ALL_GPU_READONLY & RPS_ACCESS_CPU_READ_BIT))
        {
            return ResourceAccessCategory::OtherRead;
        }

        if (accessFlags & (RPS_ACCESS_ALL_GPU_WRITE & RPS_ACCESS_CPU_WRITE_BIT))
        {
            return ResourceAccessCategory::OtherWrite;
        }

        return ResourceAccessCategory::Other;
    }

    RPS_MAYBE_UNUSED static ImU32 GetColorByAccessCategory(ResourceAccessCategory state, float alpha = 1.f)
    {
        static const ImU32 colors[static_cast<uint32_t>(ResourceAccessCategory::Count)] = {
            IM_COL32(255, 96, 98, 255),    // RenderTarget = 0,
            IM_COL32(255, 151, 255, 255),  // DepthStencilWrite,
            IM_COL32(142, 140, 215, 255),  // DepthStencilRead,
            IM_COL32(255, 186, 2, 255),    // UnorderedAccessWrite,
            IM_COL32(255, 127, 53, 255),   // CopyResolveWrite,
            IM_COL32(2, 120, 216, 255),    // CopyResolveRead,
            IM_COL32(0, 204, 105, 255),    // ShaderResourceRead,
            IM_COL32(1, 183, 197, 255),    // NonShaderResourceRead,
            IM_COL32(232, 17, 35, 255),    // RaytracingASWrite,
            IM_COL32(107, 105, 214, 255),  // RaytracingASRead,
            IM_COL32(175, 71, 194, 255),   // OtherWrite,
            IM_COL32(0, 179, 148, 255),    // OtherRead,
            IM_COL32(145, 145, 145, 255),  // Other,
        };

        RPS_ASSERT(uint32_t(state) < RPS_COUNTOF(colors));

        return colors[rpsMin(uint32_t(state), uint32_t(RPS_COUNTOF(colors)))];
    }

    static inline double GetNextMultiple(double input, double base)
    {
        const uint64_t inputInt = static_cast<uint64_t>(std::ceil(input));
        const uint64_t baseInt  = static_cast<uint64_t>(base);

        if (baseInt == 0)
            return 0.0;

        if (inputInt % baseInt == 0)
        {
            return static_cast<double>(inputInt);
        }

        return static_cast<double>((inputInt / baseInt) * baseInt + baseInt);
    }

    static inline double GetPrevMultiple(double input, double base)
    {
        const uint64_t inputInt = static_cast<uint64_t>(input);
        const uint64_t baseInt  = static_cast<uint64_t>(base);

        if (baseInt == 0)
            return 0;

        return static_cast<double>((inputInt / baseInt) * baseInt);
    }

    static inline uint64_t RoundUpToMultiplesOf(uint64_t input, uint64_t base)
    {
        return (base != 0) ? ((input + base - 1) / base * base) : input;
    }

}  // namespace rps

#endif  //RPS_VISUALIZER_UTIL_HPP
