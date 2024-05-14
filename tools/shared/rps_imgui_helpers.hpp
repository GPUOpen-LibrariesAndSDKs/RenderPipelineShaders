// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#pragma once

#ifndef IMGUI_VERSION
#error "Must only be used where imgui.h is already included"
#endif

#include <algorithm>

namespace rps
{
    namespace custom_imgui
    {
        static constexpr float SplitterHeight = 3.0f;

        static inline void DrawHorizontalSplitter(const char* name,
                                                  float&      location,
                                                  float       minLocation = 0.0f,
                                                  float       maxLocation = FLT_MAX,
                                                  float       height      = SplitterHeight)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_Separator));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_SeparatorHovered));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_SeparatorActive));

            ImGui::Button(name, ImVec2(-1.0f, height));
            const bool bActive = ImGui::IsItemActive();

            if (bActive)
            {
                //Clamp
                location = std::min(std::max(location + ImGui::GetIO().MouseDelta.y, minLocation), maxLocation);
            }

            if (bActive || ImGui::IsItemHovered())
            {
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
            }

            ImGui::PopStyleColor(3);
        }

        static inline void ChildWindowTitleBar(const char* name)
        {
            auto&        style     = ImGui::GetStyle();
            const ImVec2 size      = ImGui::CalcTextSize(name);
            const ImVec2 pos       = ImGui::GetCursorScreenPos();
            const ImVec2 avail     = ImGui::GetContentRegionAvail();
            const ImVec2 textStart = {pos.x + style.CellPadding.x, pos.y + style.CellPadding.y};
            const ImVec2 textEnd   = {textStart.x + size.x, textStart.y + size.y};
            const ImVec2 end       = {pos.x + avail.x, textEnd.y + style.CellPadding.y};

            ImGui::GetWindowDrawList()->AddRectFilled(
                pos, end, ImGui::GetColorU32(ImGui::GetStyle().Colors[ImGuiCol_TitleBgActive]));

            ImGui::GetWindowDrawList()->AddText(
                textStart, ImGui::GetColorU32(ImGui::GetStyle().Colors[ImGuiCol_Text]), name);

            ImGui::Dummy({avail.x, end.y - pos.y});
        }
    }  // namespace custom_imgui
}

