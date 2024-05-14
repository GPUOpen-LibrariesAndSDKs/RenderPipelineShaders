// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#include "rps_memory_layout_canvas.hpp"

#include "rps/runtime/common/rps_runtime.h"
#include "core/rps_util.hpp"
#include "runtime/common/rps_runtime_device.hpp"

#include "rps_imgui_helpers.hpp"

#include "rps_visualizer.hpp"

namespace rps
{
    RpsResult MemLayoutCanvas::Update(const VisualizerUpdateContext& context, uint32_t heapIdx)
    {
        m_description = context.persistentArena.StoreStr(GetHeapDescription(context, heapIdx));

        RPS_V_RETURN(m_heapSpaceZoom.SetUpperBound(context.visualizationData.heapInfos[heapIdx].size));

        m_heapIndex = heapIdx;

        m_resVisInfos.reset(&context.persistentArena);

        UpdateResourceVisuals(context.visualizationData);

        return RPS_OK;
    }

    // Get a heatmap color (blue -> green -> red) with t = [0, 1].
    static ImU32 InterpHeatmapHSV(float t)
    {
        static constexpr float bluePoint = 4.0f / 6.0f;
        // Remap to 0 - Blue, 1 - Red
        const float tRemapped = rpsClamp(bluePoint * (1.0f - t), 0.0f, 1.0f);
        return LerpHSV(tRemapped, 0.75f);
    }

    void MemLayoutCanvas::UpdateResourceVisuals(const RenderGraphVisualizationData& visData)
    {
        const auto resourceInfos = visData.resourceInfos;
        const auto resVisInfos   = visData.resourceVisInfos;

        const uint32_t lifetimeMax = rpsMax(uint32_t(visData.timelinePosToCmdIdMap.size()), 1u);

        m_maxResourceSize = 1;

        m_resVisInfos.clear();

        std::function<void(const ResourceVisInfo& resInfo)> gatherLimitsFunc = [](auto&) {};
        std::function<void(ResourceVisInfo & resInfo)>      updateColorFunc;

        switch (m_resourceColoringScheme)
        {
        case ResourceColoringScheme::Lifetime:
            updateColorFunc = [&](ResourceVisInfo& resInfo) {
                const auto& resVisInfo = resVisInfos[resInfo.resIndex];
                const float t          = float(resVisInfo.lifetimeEnd - resVisInfo.lifetimeBegin) / lifetimeMax;
                resInfo.color          = InterpHeatmapHSV(t);
            };
            break;
        case ResourceColoringScheme::Aliased:
            updateColorFunc = [&](ResourceVisInfo& resInfo) {
                resInfo.color = m_colorByAliasedPalette[resVisInfos[resInfo.resIndex].isAliased ? 0 : 1].value;
            };
            break;
        case ResourceColoringScheme::Type:
            updateColorFunc = [&](ResourceVisInfo& resInfo) {
                const int32_t resType  = int32_t(resourceInfos[resInfo.resIndex].desc.type);
                const int32_t colorIdx = (resType < RPS_RESOURCE_TYPE_COUNT) ? resType : RPS_RESOURCE_TYPE_UNKNOWN;
                resInfo.color          = m_colorByResTypeColors[colorIdx].value;
            };
            break;
        case ResourceColoringScheme::AccessUsage:
            updateColorFunc = [&](ResourceVisInfo& resInfo) {
                auto coloring =
                    GetAccessCategoryFromAccessFlags(resourceInfos[resInfo.resIndex].allAccesses.accessFlags);
                resInfo.color = GetColorByAccessCategory(coloring, 1.0f);
            };
            break;
        case ResourceColoringScheme::Size:
        default:
            gatherLimitsFunc = [&](const ResourceVisInfo& resInfo) {
                m_maxResourceSize = rpsMax(m_maxResourceSize, resourceInfos[resInfo.resIndex].allocRequirement.size);
            };
            updateColorFunc = [&](ResourceVisInfo& resInfo) {
                const float t =
                    float(double(resourceInfos[resInfo.resIndex].allocRequirement.size) / m_maxResourceSize);
                resInfo.color = InterpHeatmapHSV(t);
            };
            break;
        };

        for (uint32_t iRes = 0; iRes < resourceInfos.size(); iRes++)
        {
            auto& resourceInfo = resourceInfos[iRes];
            auto& resVisInfo   = resVisInfos[iRes];

            if ((resourceInfo.allocPlacement.heapId != m_heapIndex) || (resourceInfo.allocRequirement.size == 0) ||
                (resVisInfo.lifetimeBegin > resVisInfo.lifetimeEnd))
            {
                continue;
            }

            m_resVisInfos.push_back(ResourceVisInfo{0, iRes});

            gatherLimitsFunc(m_resVisInfos.back());
        }

        for (auto& resVisInfo : m_resVisInfos)
        {
            updateColorFunc(resVisInfo);
        }
    }

    void MemLayoutCanvas::Draw(DrawingState& state)
    {
        if (ImGui::CollapsingHeader(m_description.str))
        {
            const float frameIndent = ImGui::GetStyle().FramePadding.x * 2;

            ImGui::Indent(frameIndent);

            // One pixel offset to draw the range-end tick marker.
            static constexpr float EndRangeMarker1PixelOffset = 1.0f;

            m_viewHeight = (m_viewHeight < 0) ? (state.defaultSubViewHeight - ImGui::GetFrameHeight()) : m_viewHeight;

            const float heapSpaceHeight = m_viewHeight;
            ImGui::BeginChild(m_description.str,
                              ImVec2(0.0f, heapSpaceHeight + EndRangeMarker1PixelOffset),
                              false,
                              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

            const ImVec2 heapLeftPaneTLScr = ImGui::GetCursorScreenPos();
            const ImVec2 heapLeftPaneBRScr =
                heapLeftPaneTLScr + ImGui::GetContentRegionAvail() + ImVec2(0, -EndRangeMarker1PixelOffset);

            const float rulerHeight = heapLeftPaneBRScr.y - heapLeftPaneTLScr.y;

            m_heapSpaceZoom.SetDisplayedPixels(heapSpaceHeight);

            const float virtualRangePixels = m_heapSpaceZoom.GetTotalRangeInPixels();
            const bool  bScrollBarVisible  = virtualRangePixels > rulerHeight;
            const float scrollBarSize      = ImGui::GetStyle().ScrollbarSize;
            const float scrollBarWidth     = (bScrollBarVisible ? scrollBarSize : 0);

            const float  estimatedLabelTextWidth = rpsMax(m_maxRulerTextLen, ImGui::CalcTextSize(MaxYLabelText).x);
            const float  rulerWidth = estimatedLabelTextWidth + m_tickLength * 2 + ImGui::GetStyle().FramePadding.x * 2;
            const UIRect rulerRect  = {ImVec2(heapLeftPaneBRScr.x - rulerWidth - scrollBarWidth, heapLeftPaneTLScr.y),
                                       ImVec2(heapLeftPaneBRScr.x - scrollBarWidth, heapLeftPaneBRScr.y)};
            const UIRect rulerAndHeapViewRect = {
                ImVec2(rulerRect.start.x, rpsMax(rulerRect.start.y, state.rightPaneBodyTopY)),
                ImVec2(state.rightPaneRightScr, rpsMin(rulerRect.end.y, state.rightPaneBodyBottomY))};

            const UIRect heapViewRect = {ImVec2(state.rightPaneTopLeftScr.x, rulerRect.start.y),
                                         ImVec2(state.rightPaneRightScr, rulerRect.end.y)};

            DrawControlPanel(state);

            DrawRuler(state, rulerRect);

            auto& io = ImGui::GetIO();

            const bool bHoveringRulerAndHeapViewRect =
                ImGui::IsMouseHoveringRect(rulerAndHeapViewRect.start, rulerAndHeapViewRect.end, false);

            const bool bHoveringHeapViewRect = ImGui::IsMouseHoveringRect(heapViewRect.start, heapViewRect.end, false);

            bool bZooming = false;
            bool bMoving  = false;

            if (bHoveringRulerAndHeapViewRect)
            {
                const float wheel = io.MouseWheel;
                if (io.KeyShift && (abs(wheel) > 0.1f))
                {
                    // To prevent scrollbar precision issue when zoom is too big,
                    // don't allow zoom in when 4 bytes - 1 pixel.
                    if ((wheel < 0) || (m_heapSpaceZoom.UnitsToPixels(4) < 1))
                    {
                        m_heapSpaceZoom.ZoomByMultiplier(powf(1.1f, rpsClamp(wheel, -10.0f, 10.0f)),
                                                         state.mousePosScr.y - rulerRect.start.y);
                        io.MouseWheel = 0.0f;

                        bZooming = true;
                    }
                }
            }

            // Ctrl + B : Zoom to selection
            if (state.settings.bDrawSelector && m_heapSpaceSelector.HasSelection() &&
                !ImGui::IsMouseDown(ImGuiMouseButton_Left) && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_B))
            {
                const U64Vec2 selectionRangeUnits = m_heapSpaceSelector.GetSelectionRangeOrdered();
                m_heapSpaceZoom.ZoomToUnitRange(selectionRangeUnits.x, selectionRangeUnits.y);

                bZooming = true;
            }

            //Vertical Movement
            const int stepsUp =
                ImGui::GetKeyPressedAmount(ImGuiKey_W, OverlayState::ButtonRepeatDelay, OverlayState::ButtonRepeatRate);
            const int stepsDown =
                ImGui::GetKeyPressedAmount(ImGuiKey_S, OverlayState::ButtonRepeatDelay, OverlayState::ButtonRepeatRate);
            const int stepsTotal = stepsDown - stepsUp;
            bMoving              = stepsTotal != 0;
            m_heapSpaceZoom.MoveByUnits(stepsTotal * CalcKeyMoveMemStep(m_heapSpaceZoom.GetVisibleRangeUnits()));

            const UIRect scrollBarRect = {
                ImVec2(rulerRect.end.x, rulerRect.start.y),
                ImVec2(rulerRect.end.x + (bScrollBarVisible ? scrollBarSize : 0), rulerRect.end.y)};

            const float scrollWindowWidth = scrollBarRect.end.x - rulerRect.start.x;

            ImGui::SetNextWindowContentSize(ImVec2(scrollWindowWidth, virtualRangePixels));

            StrBuilder<> sb;
            sb.AppendFormat("##HeapScrollY_%d", m_heapIndex);

            ImGui::SetCursorScreenPos(rulerRect.start);
            ImGui::BeginChild(sb.c_str(), ImVec2(scrollWindowWidth, rulerHeight), false, ImGuiWindowFlags_NoBackground);
            ImGui::GetWindowDrawList()->AddLine(
                rulerRect.start, ImVec2(rulerRect.start.x, rulerRect.end.y), ImGui::GetColorU32(ImGuiCol_Separator));

            if (bZooming || bMoving)
            {
                ImGui::SetScrollY(m_heapSpaceZoom.GetScrollInPixels());
            }
            else
            {
                const float scroll = ImGui::GetScrollY();
                m_heapSpaceZoom.SetScrollInPixels(scroll);
            }

            const bool bMouseOnScrollBar = ImGui::IsMouseHoveringRect(scrollBarRect.start, scrollBarRect.end, false);

            ImGui::EndChild();

            ImGui::EndChild();

            ImGui::Unindent(frameIndent);

            auto pDrawList = ImGui::GetWindowDrawList();

            // Draw resource rects last outside of control panel child window
            // to prevent clipping when control panel is invisible due to vertical resizing.
            ImGui::PushClipRect(ImVec2(state.rightPaneTopLeftScr.x, rulerAndHeapViewRect.start.y),
                                ImVec2(state.rightPaneRightScr, rulerAndHeapViewRect.end.y + 1),
                                false);

            DrawHeapResources(state, heapViewRect);

            ImGui::PopClipRect();

            if (bHoveringRulerAndHeapViewRect)
            {
                const ImU32 cursorLineColor = ImGui::GetColorU32(ImGuiCol_Separator);

                const float rulerLeftClipped =
                    rpsMin(state.rightPaneTopLeftScr.x, rpsMax(rulerRect.start.x, heapLeftPaneTLScr.x));
                pDrawList->PushClipRect(rulerRect.start, state.rightPaneBottomRightScr);
                pDrawList->AddLine(ImVec2(rulerLeftClipped, state.mousePosScr.y),
                                   ImVec2(state.rightPaneRightScr, state.mousePosScr.y),
                                   cursorLineColor);
                pDrawList->PopClipRect();

                const uint64_t timelineHoveringPos =
                    m_heapSpaceZoom.Pick(rpsClamp(state.mousePosScr.y - rulerRect.start.y, 0.0f, rulerHeight));

                //Uses side effect of SetActiveID to cancel window moves if mouse is clicked over the right pane.
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !bMouseOnScrollBar)
                {
                    m_heapSpaceSelector.BeginDrag(timelineHoveringPos);
                    ImGui::SetActiveID(ImGui::GetActiveID(), nullptr);
                }
                else if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f) && m_heapSpaceSelector.IsDragging())
                {
                    if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
                    {
                        const bool draggingRight = (m_heapSpaceSelector.GetSelectionRange().x <= timelineHoveringPos);
                        m_heapSpaceSelector.DragTo(timelineHoveringPos + (draggingRight ? 1 : 0));
                    }
                    ImGui::SetActiveID(ImGui::GetActiveID(), nullptr);
                }
            }

            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            {
                m_heapSpaceSelector.EndDrag();
            }

            if (!bHoveringHeapViewRect && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                m_heapSpaceSelector.EndDrag();
                m_heapSpaceSelector.ClearSelection();
            }

            if (m_heapSpaceSelector.HasSelection() && state.settings.bDrawSelector)
            {
                ImGui::PushClipRect(state.rightPaneTopLeftScr, state.rightPaneBottomRightScr, false);

                const ImU32 selectionOverlayColor = ImGui::GetColorU32(ImGuiCol_TextSelectedBg, 0.4f);

                const U64Vec2  selectedRange     = m_heapSpaceSelector.GetSelectionRangeOrdered();
                const uint64_t visibleRangeBegin = m_heapSpaceZoom.GetVisibleRangeBegin();
                const ImVec2   selectedRangeScr  = {
                    rulerRect.start.y + m_heapSpaceZoom.UnitsToPixels(selectedRange.x - visibleRangeBegin),
                    rulerRect.start.y + m_heapSpaceZoom.UnitsToPixels(selectedRange.y - visibleRangeBegin)};

                pDrawList->AddRectFilled(ImVec2(rulerRect.start.x, selectedRangeScr.x),
                                         ImVec2(state.rightPaneRightScr, selectedRangeScr.y),
                                         selectionOverlayColor);

                ImGui::PopClipRect();
            }

            custom_imgui::DrawHorizontalSplitter(
                StrBuilder<>("##HeapHSplitter_").AppendFormat("%u", m_heapIndex).c_str(), m_viewHeight);
        }
    }

    void MemLayoutCanvas::DrawControlPanel(DrawingState& state)
    {
        static constexpr const char* resourceColoringSchemeNames[int32_t(ResourceColoringScheme::Count)]{
            "Size",
            "Lifetime",
            "Aliased",
            "Type",
            "AccessUsage",
        };
        int currColoring = int(m_resourceColoringScheme);
        ImGui::Text("Color by");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::CalcTextSize("AccessUsage").x);
        if (ImGui::Combo(
                "##ColorBy", &currColoring, resourceColoringSchemeNames, int32_t(ResourceColoringScheme::Count)))
        {
            m_resourceColoringScheme = ResourceColoringScheme(currColoring);
            UpdateResourceVisuals(state.visData);
        }

        auto drawResourcePalette = [](ConstArrayRef<ColorNameValue> colorScheme) {
            for (uint32_t i = 0; i < colorScheme.size(); i++)
            {
                ImGui::PushID(i);
                ImGui::ColorButton("##ColorScheme",
                                   ImGui::ColorConvertU32ToFloat4(colorScheme[i].value),
                                   ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoTooltip);
                ImGui::SameLine();
                ImGui::TextUnformatted(colorScheme[i].name);
                ImGui::PopID();
            }
        };

        auto drawColoringGradient = [](const StrRef minValueLabel, const StrRef maxValueLabel) {
            static constexpr uint32_t sections = 18;

            ImDrawList* const pDrawList = ImGui::GetWindowDrawList();

            const float frameHeight   = ImGui::GetFrameHeight();
            const float sectionHeight = frameHeight * 8 / float(sections);

            const ImVec2 drawCursorPos = ImGui::GetCursorScreenPos();

            ImVec2 sectionTopLeft = drawCursorPos;
            ImU32  topCol         = InterpHeatmapHSV(0.0f);
            for (uint32_t i = 0; i < sections; i++)
            {
                const ImU32 botCol = InterpHeatmapHSV((i + 1) / float(sections));
                pDrawList->AddRectFilledMultiColor(sectionTopLeft,
                                                   sectionTopLeft + ImVec2(frameHeight, sectionHeight),
                                                   topCol,
                                                   topCol,
                                                   botCol,
                                                   botCol);

                sectionTopLeft.y += sectionHeight;
                topCol = botCol;
            }

            pDrawList->AddRect(drawCursorPos,
                               sectionTopLeft + ImVec2(frameHeight, 0),
                               IM_COL32(255, 255, 255, 191),
                               0.0f,
                               ImDrawFlags_None,
                               0.5f);

            const float labelOffsetX = frameHeight + ImGui::GetStyle().FramePadding.x;

            if (minValueLabel)
            {
                ImGui::SetCursorScreenPos(drawCursorPos + ImVec2(labelOffsetX, 0));
                ImGui::TextUnformatted(minValueLabel.str);
            }

            if (maxValueLabel)
            {
                ImGui::SetCursorScreenPos(sectionTopLeft + ImVec2(labelOffsetX, -ImGui::GetFontSize()));
                ImGui::TextUnformatted(maxValueLabel.str);
            }
        };

        auto           pDrawList = ImGui::GetWindowDrawList();
        StrBuilder<64> sbMin, sbMax;

        switch (m_resourceColoringScheme)
        {
        case ResourceColoringScheme::Size:
        {
            auto memSizeFmt = FormatMemorySize(m_maxResourceSize);
            sbMin.Append("0 ").Append(memSizeFmt.second);
            sbMax.AppendFormat("%.2f ", memSizeFmt.first).Append(memSizeFmt.second);
            drawColoringGradient(sbMin.GetStr(), sbMax.GetStr());
        }
        break;
        case ResourceColoringScheme::Lifetime:
        {
            sbMax.AppendFormat("%u", rpsMax(uint32_t(state.visData.timelinePosToCmdIdMap.size()), 1u));
            drawColoringGradient("0", sbMax.GetStr());
        }
        break;
        case ResourceColoringScheme::Aliased:
            // TODO: allow customization
            drawResourcePalette({m_colorByAliasedPalette, RPS_COUNTOF(m_colorByAliasedPalette)});
            break;
        case ResourceColoringScheme::Type:
            drawResourcePalette({m_colorByResTypeColors, RPS_COUNTOF(m_colorByResTypeColors)});
            break;
        case ResourceColoringScheme::AccessUsage:
        {
#define RPS_VISUALIZER_DECL_COLOR_BY_ACCESS_ENTRY(Name) {#Name, GetColorByAccessCategory(ResourceAccessCategory::Name)}
            ColorNameValue colorByAccessColors[uint32_t(ResourceAccessCategory::Count)] = {
                RPS_VISUALIZER_DECL_COLOR_BY_ACCESS_ENTRY(RenderTarget),
                RPS_VISUALIZER_DECL_COLOR_BY_ACCESS_ENTRY(DepthStencilWrite),
                RPS_VISUALIZER_DECL_COLOR_BY_ACCESS_ENTRY(DepthStencilRead),
                RPS_VISUALIZER_DECL_COLOR_BY_ACCESS_ENTRY(UnorderedAccessWrite),
                RPS_VISUALIZER_DECL_COLOR_BY_ACCESS_ENTRY(CopyResolveWrite),
                RPS_VISUALIZER_DECL_COLOR_BY_ACCESS_ENTRY(CopyResolveRead),
                RPS_VISUALIZER_DECL_COLOR_BY_ACCESS_ENTRY(ShaderResourceRead),
                RPS_VISUALIZER_DECL_COLOR_BY_ACCESS_ENTRY(NonShaderResourceRead),
                RPS_VISUALIZER_DECL_COLOR_BY_ACCESS_ENTRY(RaytracingASWrite),
                RPS_VISUALIZER_DECL_COLOR_BY_ACCESS_ENTRY(RaytracingASRead),
                RPS_VISUALIZER_DECL_COLOR_BY_ACCESS_ENTRY(OtherWrite),
                RPS_VISUALIZER_DECL_COLOR_BY_ACCESS_ENTRY(OtherRead),
                RPS_VISUALIZER_DECL_COLOR_BY_ACCESS_ENTRY(Other),
            };
#undef RPS_VISUALIZER_DECL_COLOR_BY_ACCESS_ENTRY
            drawResourcePalette({colorByAccessColors, RPS_COUNTOF(colorByAccessColors)});
        }
        break;
        default:
            break;
        };
    }

    void MemLayoutCanvas::DrawRuler(const DrawingState& state, const UIRect& rulerRect)
    {
        static constexpr uint32_t MajorTickIntervalCount = 8;

        const ImVec2 rulerSize = rulerRect.GetSize();

        ImDrawList* pDrawList = ImGui::GetWindowDrawList();

        ImGui::PushClipRect(rulerRect.start, rulerRect.end + ImVec2{0, 1}, true);

        ImGui::SetCursorScreenPos(rulerRect.start);

        const ImU32 textColor = ImGui::GetColorU32(ImGuiCol_Text);

        const ImVec2 rulerStart = {rulerRect.end.x, rulerRect.start.y};
        pDrawList->AddLine(rulerStart - ImVec2{1, 0}, rulerRect.end - ImVec2{1, -1}, textColor);

        const ZoomState& zoom = m_heapSpaceZoom;

        const uint64_t tickInterval = rpsRoundUpToPowerOfTwo(zoom.GetTickInterval());

        if (tickInterval > 0)
        {
            const float tickIntervalPixels = zoom.UnitsToPixels(tickInterval);

            const float textMaxX = rulerRect.end.x - m_tickLength * 2 - ImGui::GetStyle().FramePadding.x;

            m_maxRulerTextLen = 0;

            const uint64_t firstTick  = RoundUpToMultiplesOf(zoom.GetVisibleRangeBegin(), tickInterval);
            const uint64_t maxTickIdx = (zoom.GetVisibleRangeEnd() > firstTick)
                                            ? ((zoom.GetVisibleRangeEnd() - 1 - firstTick) / tickInterval)
                                            : 0;

            const float fontSize = ImGui::GetFontSize();

            float prevTextBottomY = -10000.0f;

            StrBuilder<> prevLabelText;

            auto drawTick = [&](uint64_t tickValue, bool bForceMajorTick) {
                uint64_t   tickMultiplier = tickValue / tickInterval;
                const bool bMajorTick     = (((tickMultiplier) % MajorTickIntervalCount) == 0) || bForceMajorTick;
                const bool bMediumTick    = ((tickMultiplier % (MajorTickIntervalCount / 2)) == 0);

                const bool bSparseMinorTick = ((tickInterval == 1) && (tickIntervalPixels > 50.0f));

                const float tickPosYOffset = zoom.UnitsToPixels(tickValue - zoom.GetVisibleRangeBegin());
                const float tickPosY       = rulerRect.start.y + tickPosYOffset;
                const float tickPosLeft = rulerRect.end.x - m_tickLength * (bMajorTick ? 2 : (bMediumTick ? 1.6f : 1));

                pDrawList->AddLine(ImVec2(tickPosLeft, tickPosY), ImVec2(rulerRect.end.x - 1, tickPosY), textColor);

                if (bMajorTick || bSparseMinorTick)
                {
                    const float estimatedTextTop =
                        rpsClamp(tickPosY - fontSize * 0.5f, rulerRect.start.y, rulerRect.end.y - fontSize);

                    const UIRect labelRect = {ImVec2(rulerRect.start.x, estimatedTextTop),
                                              ImVec2(rulerRect.end.x, estimatedTextTop + fontSize)};

                    const bool bHoveringNearLabel = labelRect.Contains(state.mousePosScr);

                    if (bHoveringNearLabel && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
                    {
                        m_bShowAddress = !m_bShowAddress;
                    }

                    StrBuilder<> sb;
                    if (m_bShowAddress)
                        sb.AppendFormat("0x%" PRIx64, tickValue);
                    else
                        FormatMemorySize(sb.AsPrinter(), tickValue, tickInterval * MajorTickIntervalCount);

                    const ImVec2 textSize =
                        ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, sb.c_str(), sb.c_str() + sb.Length());

                    const float textLeft = textMaxX - textSize.x;
                    const float textTop =
                        rpsClamp(tickPosY - textSize.y * 0.5f, rulerRect.start.y, rulerRect.end.y - fontSize);

                    // Avoid overlapping
                    if (((textTop - prevTextBottomY) > 0) && (prevLabelText.GetStr() != sb.GetStr()))
                    {
                        m_maxRulerTextLen = rpsMax(textSize.x, m_maxRulerTextLen);

                        pDrawList->AddText(ImVec2(textLeft, textTop), textColor, sb.c_str(), sb.c_str() + sb.Length());

                        prevTextBottomY = textTop + textSize.y;

                        prevLabelText = sb;
                    }
                }
            };

            if (firstTick != zoom.GetVisibleRangeBegin())
            {
                drawTick(zoom.GetVisibleRangeBegin(), true);
            }

            for (uint64_t tickIdx = 0; tickIdx <= maxTickIdx; tickIdx++)
            {
                uint64_t tick = firstTick + tickInterval * tickIdx;
                drawTick(tick, false);
            }

            drawTick(zoom.GetVisibleRangeEnd(), true);
        }

        ImGui::PopClipRect();
    }

    void MemLayoutCanvas::DrawHeapResources(const DrawingState& state, const UIRect& viewRect)
    {
        ImDrawList* const pDrawList = ImGui::GetWindowDrawList();

        const ImU32 boundsLineColor = ImGui::GetColorU32(ImGuiCol_Text);

        pDrawList->AddLine(viewRect.start, ImVec2(viewRect.end.x, viewRect.start.y), boundsLineColor);
        pDrawList->AddLine(ImVec2(viewRect.start.x, viewRect.end.y), viewRect.end, boundsLineColor);

        const auto resourceInfos = state.visData.resourceInfos;
        const auto resVisInfos   = state.visData.resourceVisInfos;

        for (uint32_t iVisResIdx = 0; iVisResIdx < m_resVisInfos.size(); iVisResIdx++)
        {
            const auto& resVisInfoPriv = m_resVisInfos[iVisResIdx];

            const auto& resourceInfo = resourceInfos[resVisInfoPriv.resIndex];
            const auto& resVisInfo   = resVisInfos[resVisInfoPriv.resIndex];

            RPS_ASSERT(!((resourceInfo.allocPlacement.heapId != m_heapIndex) ||
                         (resourceInfo.allocRequirement.size == 0) ||
                         (resVisInfo.lifetimeBegin > resVisInfo.lifetimeEnd)));

            const ImVec2 rectTL = {
                state.rightPaneTopLeftScr.x + state.timelineZoom.UnitToPixelOffset(resVisInfo.lifetimeBegin),
                viewRect.start.y + m_heapSpaceZoom.UnitToPixelOffset(resourceInfo.allocPlacement.offset)};

            const ImVec2 rectBR = {
                state.rightPaneTopLeftScr.x + state.timelineZoom.UnitToPixelOffset(resVisInfo.lifetimeEnd),
                viewRect.start.y + m_heapSpaceZoom.UnitToPixelOffset(resourceInfo.allocPlacement.offset +
                                                                     resourceInfo.allocRequirement.size)};

            const bool bHovering = ImGui::IsMouseHoveringRect(rectTL, rectBR, true);

            pDrawList->AddRectFilled(rectTL, rectBR, resVisInfoPriv.color);
            pDrawList->AddRect(rectTL, rectBR, IM_COL32(255, 255, 255, 191), 0.0f, ImDrawFlags_None, 1.0f);

            if (bHovering && resourceInfo.name && state.settings.bDrawHeapTooltips)
            {
                DrawHeapResourceToolTip(state, resVisInfoPriv);
            }
        }
    }

    void MemLayoutCanvas::DrawHeapResourceToolTip(const DrawingState& state, const ResourceVisInfo& visInfo)
    {
        auto resourceInfo = state.visData.resourceInfos[visInfo.resIndex];
        auto resVisInfo   = state.visData.resourceVisInfos[visInfo.resIndex];

        StrBuilder<> sb;

        ImGui::BeginTooltip();
        ImGui::TextUnformatted(resourceInfo.name);

        ImGui::Spacing();
        sb.AppendFormat("Size   : 0x%" PRIx64, resourceInfo.allocRequirement.size);
        ImGui::TextUnformatted(sb.c_str());

        sb.Reset();
        sb.AppendFormat("Align  : 0x%x", resourceInfo.allocRequirement.alignment);
        ImGui::TextUnformatted(sb.c_str());

        sb.Reset();
        sb.AppendFormat("Offset : 0x%" PRIx64 ", %s",
                        resourceInfo.allocPlacement.offset,
                        resVisInfo.isAliased ? "Aliased" : "Not Aliased");
        ImGui::TextUnformatted(sb.c_str());

        ImGui::Spacing();

        sb.Reset();
        sb.AppendFormat("Lifetime : [%u, %u]", resVisInfo.lifetimeBegin, resVisInfo.lifetimeEnd);

        sb.Reset();
        sb.Append("Accesses : [ ");
        AccessAttr(resourceInfo.allAccesses).Print(sb.AsPrinter());
        sb.Append(" ]");
        ImGui::TextUnformatted(sb.c_str());

        ImGui::Spacing();
        sb.Reset();
        const ResourceDescPacked resDesc{resourceInfo.desc};
        resDesc.Print(sb.AsPrinter());
        ImGui::TextUnformatted(sb.c_str());

        ImGui::EndTooltip();
    }

    StrRef MemLayoutCanvas::GetHeapDescription(const VisualizerUpdateContext& context, uint32_t heapIndex) const
    {
        StrBuilder<> strBuilder;
        strBuilder.AppendFormat("Heap %u : ", heapIndex);

        const uint32_t       memTypeIndex = context.visualizationData.heapInfos[heapIndex].memoryTypeIndex;
        const RuntimeDevice* pRtDevice    = RuntimeDevice::Get(context.pRenderGraph->GetDevice());

        const size_t prevLength = strBuilder.Length();
        pRtDevice->DescribeMemoryType(memTypeIndex, strBuilder.AsPrinter());
        if (prevLength == strBuilder.Length())
        {
            strBuilder.PopBack(3);
        }

        return context.persistentArena.StoreStr(strBuilder.GetStr());
    }

    uint64_t MemLayoutCanvas::CalcKeyMoveMemStep(uint64_t memRange)
    {
        //Round down to power of 2
        constexpr uint64_t VerticalScrollBase = 128;
        return 1ull << (64 - rpsFirstBitHigh(memRange / VerticalScrollBase));
    }
}  // namespace rps
