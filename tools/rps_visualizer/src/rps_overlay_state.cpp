// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#include "rps_overlay_state.hpp"

#include "rps_visualizer_util.hpp"
#include "rps_visualizer.hpp"

namespace rps
{
    // Default units of the timeline ruler. Purely cosmetic.
    static constexpr uint32_t DefaultTimelineUnits = 100;

    static const char* const UserSettingsLabel = "Visualizer Settings";
    //Separate settings file due to decoupled lifetimes of Visualizer and ImGui
    static const char* const SettingsFileName = "rps_visualizer_settings.ini";

    static void* OverlayImGuiCbReadOpen(ImGuiContext* ctx, ImGuiSettingsHandler* pHandler, const char* name)
    {
        if (pHandler == nullptr)
            return nullptr;

        return pHandler->UserData;
    }

    static void OverlayImGuiCbReadLine(ImGuiContext* ctx, ImGuiSettingsHandler* pHandler, void* entry, const char* line)
    {
        if ((pHandler == nullptr) || (line == nullptr))
            return;

        const size_t  nameLen       = strlen(line);
        DrawSettings* pDrawSettings = static_cast<DrawSettings*>(pHandler->UserData);
        if (nameLen == 0)
        {
            return;
        }

        const char* findResult = strchr(line, '=');
        if (findResult == nullptr)
            return;  //Skip line if no '=' char is found.

        pDrawSettings->ForMembers([&](const char* name, bool& value) {
            if (strncmp(name, line, nameLen) == 0)
            {
                value = std::stoi(findResult + 1);
                return false;
            }

            return true;
        });
    }

    static void OverlayImGuiCbWriteAll(ImGuiContext* pCtx, ImGuiSettingsHandler* pHandler, ImGuiTextBuffer* buf)
    {
        if (pHandler == nullptr)
            return;
        DrawSettings* pDrawSettings = static_cast<DrawSettings*>(pHandler->UserData);
        buf->appendf("[%s][%s]\n", UserSettingsLabel, UserSettingsLabel);
        pDrawSettings->ForMembers([&](const char* name, bool& value) {
            buf->appendf("%s=%d\n", name, value);
            return true;
        });
    }

    class ImGuiSerializer
    {
        RPS_CLASS_NO_COPY_MOVE(ImGuiSerializer);

    public:
        ImGuiSerializer(DrawSettings& drawSettings)
        {
            ImGuiSettingsHandler settingsHandler = {};

            settingsHandler.TypeName   = UserSettingsLabel;
            settingsHandler.TypeHash   = ImHashStr(UserSettingsLabel);
            settingsHandler.ReadOpenFn = &OverlayImGuiCbReadOpen;
            settingsHandler.ReadLineFn = &OverlayImGuiCbReadLine;
            settingsHandler.WriteAllFn = &OverlayImGuiCbWriteAll;
            settingsHandler.UserData   = &drawSettings;

            ImGui::AddSettingsHandler(&settingsHandler);
        }

        ~ImGuiSerializer()
        {
            ImGui::RemoveSettingsHandler(UserSettingsLabel);
        }

        void Load(const char* path)
        {
            ImGui::LoadIniSettingsFromDisk(path);
        }

        void Save(const char* path)
        {
            ImGui::SaveIniSettingsToDisk(path);
        }
    };

    OverlayState::OverlayState(const Device& device, const RpsVisualizerCreateInfo& createInfo)
        : m_heapStates(0, &device.Allocator())
        , m_bChildWindow(!!(createInfo.flags & RPS_VISUALIZER_CREATE_CHILD_WINDOW_BIT))
    {
        m_timelineZoom.SetUpperBound(DefaultTimelineUnits);

        constexpr char separator = '/';

        m_imGuiConfigFilePath = (createInfo.settingsFolderPath == nullptr)
                                    ? SettingsFileName
                                    : (std::string(createInfo.settingsFolderPath) + separator) + SettingsFileName;
    }

    OverlayState::~OverlayState()
    {
        if (ImGui::GetCurrentContext())
        {
            ImGuiSerializer(m_drawSettings).Save(m_imGuiConfigFilePath.c_str());
        }
    }

    RpsResult OverlayState::Draw(const RenderGraphVisualizationData& visData)
    {
        RpsResult result = RPS_OK;

        if (ImGui::GetCurrentContext() == nullptr)
        {
            //No active ImGui context, e.g. because of missing ImGui initialization.
            return RPS_ERROR_INVALID_OPERATION;
        }

        if (m_bPendingSettingsLoad)
        {
            ImGuiSerializer(m_drawSettings).Load(m_imGuiConfigFilePath.c_str());
            m_bPendingSettingsLoad = false;
        }

        if (!m_bChildWindow)
        {
            ImGui::GetIO().ConfigWindowsMoveFromTitleBarOnly = true;
            ImGui::Begin("Rps Visualizer");
        }

        DrawingState drawingState          = {visData, m_drawSettings, m_timelineZoom};
        drawingState.rightPaneTopLeftScr.y = ImGui::GetCursorScreenPos().y;
        drawingState.mousePosScr           = ImGui::GetIO().MousePos;

        static constexpr ImGuiTableFlags visViewRootTableFlags = ImGuiTableFlags_BordersOuter |
                                                                 ImGuiTableFlags_BordersInnerV |
                                                                 ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;

        bool bSettingsChanged = false;

        const ImVec2 framePadding = ImGui::GetStyle().FramePadding;
        const ImVec2 tableSize    = m_bChildWindow ? ImVec2{-framePadding.x - 1, -framePadding.y - 1} : ImVec2{};
        if (ImGui::BeginTable("VisualizerViewRoot", 2, visViewRootTableFlags, tableSize))
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            drawingState.resourceHeaderTopY = ImGui::GetCursorPosY();

            m_resLifetimeCanvas.Draw(drawingState);

            if (ImGui::CollapsingHeader("Heaps"))
            {
                DrawHeaps(drawingState);
            }

            if (ImGui::CollapsingHeader("Graph"))
            {
                m_graphCanvas.Draw(drawingState);
            }

            ImGui::EndTable();

            DrawRuler(drawingState);

            bSettingsChanged |= DrawSettingsSelection(drawingState);
        }

        if (!m_bChildWindow)
        {
            ImGui::End();
        }

        if (bSettingsChanged)
        {
            ImGuiSerializer(m_drawSettings).Save(m_imGuiConfigFilePath.c_str());
        }

        return result;
    }

    bool OverlayState::DrawSettingsSelection(DrawingState& state)
    {
        bool bSettingsChanged = false;

        ImGui::SetCursorScreenPos(state.rightPaneTopLeftScr);

        ImGui::BeginGroup();

        ImGui::NewLine();
        ImGui::NewLine();

        constexpr ImVec2 SettingsPanelMinSize = {200, 200};

        m_settingsPanelSize = ImMax(m_settingsPanelSize, SettingsPanelMinSize);

        ImVec4 childBg = ImGui::GetStyleColorVec4(ImGuiCol_ChildBg);
        childBg.w      = 1.0f;

        ImGui::PushStyleColor(ImGuiCol_ChildBg, childBg);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2());
        const float widgetSize = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2;
        ImGui::BeginChild("DrowDownDepthPadding", {widgetSize, widgetSize});
        ImGui::BeginChild("DrowDownDepthPadding2");
        ImGui::BeginChild("DrowDownDepthPadding3");
        ImGui::ArrowButton("SettingsSelection", m_drawSettings.bDrawSettingsSelector ? ImGuiDir_Down : ImGuiDir_Right);
        ImGui::SameLine();
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            const ImVec2 currentPos = ImGui::GetCursorScreenPos();
            const bool   hovered    = ImGui::IsItemHovered(ImGuiHoveredFlags_RectOnly);
            if (hovered)
            {
                m_drawSettings.bDrawSettingsSelector = !m_drawSettings.bDrawSettingsSelector;
                bSettingsChanged                     = true;
            }
        }
        ImGui::EndChild();
        ImGui::EndChild();
        ImGui::EndChild();
        ImGui::PopStyleVar(1);
        ImGui::PopStyleColor();

        if (m_drawSettings.bDrawSettingsSelector)
        {
            ImGui::SameLine();

            ImVec4 childBg = ImGui::GetStyleColorVec4(ImGuiCol_ChildBg);
            childBg.w      = 1.0f;

            ImGui::PushStyleColor(ImGuiCol_ChildBg, childBg);
            //Multiple nested children to make sure the window is not overdrawn by the pDrawlist->Add* calls in other windows
            const ImVec2 windowPadding = ImGui::GetStyle().WindowPadding;
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2());
            ImGui::BeginChild("SettingsSelectorDepthPadding", m_settingsPanelSize);
            ImGui::BeginChild("SettingsSelectorDepthPadding1");
            ImGui::BeginChild("SettingsSelectorDepthPadding2");
            ImGui::BeginChild("SettingsSelectorDepthPadding3");
            ImGui::PopStyleVar(1);
            ImGui::BeginChild("SettingsSelectorDepthPadding4", m_settingsPanelSize, true);

            UIRect contentRect = {ImGui::GetCursorScreenPos(), ImGui::GetCursorScreenPos()};

            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !bSettingsChanged &&
                !ImGui::IsMouseHoveringRect(
                    contentRect.start - windowPadding, contentRect.start + m_settingsPanelSize - windowPadding, false))
            {
                bSettingsChanged |= m_drawSettings.bDrawSettingsSelector;
                m_drawSettings.bDrawSettingsSelector = false;
            }

            auto addCheckbox = [&](const char* label, bool* pValue) {
                bSettingsChanged |= ImGui::Checkbox(label, pValue);
                contentRect.end = ImMax(contentRect.end, ImGui::GetItemRectMax());
            };

            if (ImGui::CollapsingHeader("Resources", ImGuiTreeNodeFlags_DefaultOpen))
            {
                addCheckbox("Draw Accesses", &m_drawSettings.bDrawResourceAccesses);
                addCheckbox("Draw Transitions", &m_drawSettings.bDrawResourceTransitions);
                addCheckbox("Draw Connectors", &m_drawSettings.bDrawResourceConnectors);
                addCheckbox("Draw Tooltips## for Resources", &m_drawSettings.bDrawResourceTooltips);
                addCheckbox("Draw subresource data lifetime markers",
                            &m_drawSettings.bDrawSubResourceDataLifetimeMarkers);
            }

            if (ImGui::CollapsingHeader("Heaps", ImGuiTreeNodeFlags_DefaultOpen))
            {
                addCheckbox("Draw Tooltips## for Heaps", &m_drawSettings.bDrawHeapTooltips);
            }

            if (ImGui::CollapsingHeader("Graph", ImGuiTreeNodeFlags_DefaultOpen))
            {
                addCheckbox("Draw Tooltips## for Graph", &m_drawSettings.bDrawGraphTooltips);
            }

            ImGui::EndChild();
            ImGui::EndChild();
            ImGui::EndChild();
            ImGui::EndChild();
            ImGui::EndChild();

            ImGui::PopStyleColor();

            m_settingsPanelSize = ImMax(m_settingsPanelSize, contentRect.GetSize() + windowPadding * 2);
        }

        ImGui::EndGroup();

        return bSettingsChanged;
    }

    RpsResult OverlayState::Update(const VisualizerUpdateContext& context)
    {
        m_resLifetimeCanvas.Update(context);

        auto heapInfos = context.visualizationData.heapInfos;

        m_heapStates.resize(heapInfos.size());

        for (uint32_t heapIdx = 0; heapIdx < heapInfos.size(); heapIdx++)
        {
            RPS_V_RETURN(m_heapStates[heapIdx].Update(context, heapIdx));
        }

        m_graphCanvas.Update(context);

        m_timelineZoom.SetUpperBound(context.visualizationData.timelinePosToCmdIdMap.size());

        return RPS_OK;
    }

    void OverlayState::DrawHeaps(DrawingState& state)
    {
        ImGui::Indent();
        ImGui::BeginGroup();

        for (uint32_t heapIndex = 0; heapIndex < m_heapStates.size(); ++heapIndex)
        {
            m_heapStates[heapIndex].Draw(state);
        }

        ImGui::EndGroup();
        ImGui::Unindent();
    }

    void OverlayState::DrawRuler(DrawingState& state)
    {
        const ImVec2 rulerTopLeft     = state.rightPaneTopLeftScr;
        const ImVec2 rulerBottomRight = ImVec2(state.rightPaneRightScr, state.rightPaneBodyTopY - 1);
        const ImVec2 rulerSize = ImVec2(rulerBottomRight.x - rulerTopLeft.x, rulerBottomRight.y - rulerTopLeft.y);

        m_timelineZoom.SetDisplayedPixels(rulerSize.x - 1);

        ImDrawList* pDrawList = ImGui::GetWindowDrawList();

        bool bZooming = false;
        bool bMoving  = false;

        auto& io = ImGui::GetIO();
        if (ImGui::IsMouseHoveringRect(state.rightPaneTopLeftScr, state.rightPaneBottomRightScr))
        {
            float wheel = io.MouseWheel;
            if (io.KeyCtrl && (abs(wheel) > 0.1f))
            {
                m_timelineZoom.ZoomByMultiplier(powf(1.1f, rpsClamp(wheel, -10.0f, 10.0f)),
                                                state.mousePosScr.x - rulerTopLeft.x);
                io.MouseWheel = 0.0f;

                bZooming = true;
            }
        }

        ImGui::PushClipRect(rulerTopLeft, rulerBottomRight, true);

        ImGui::SetCursorScreenPos(state.rightPaneTopLeftScr);
        ImGui::BeginChild("Ruler", rulerSize);

        const ImU32 textColor = ImGui::GetColorU32(ImGuiCol_Text);

        const ImVec2 rulerStart = {rulerTopLeft.x, rulerBottomRight.y - 1};
        pDrawList->AddLine(rulerStart, ImVec2(rulerBottomRight.x, rulerStart.y), textColor);

        const ZoomState& zoom = m_timelineZoom;

        const uint64_t tickInterval       = zoom.GetTickInterval();
        const float    tickIntervalPixels = zoom.UnitsToPixels(tickInterval);

        float tickLength = 5.0f;

        const float labelMinSpacing = ImGui::GetFontSize() * 0.5f;

        // Reserve space for ending marker label
        StrBuilder<> endingMarkSb;
        endingMarkSb.AppendFormat("%" PRIu64, zoom.GetVisibleRangeEnd());
        const ImVec2 endingMarkTextSize = ImGui::GetFont()->CalcTextSizeA(
            ImGui::GetFontSize(), FLT_MAX, 0.0f, endingMarkSb.c_str(), endingMarkSb.c_str() + endingMarkSb.Length());
        const float labelTextCutoffX = rulerBottomRight.x - endingMarkTextSize.x - labelMinSpacing;

        float prevLabelRight = -10000.0f;

        auto drawTick = [&](uint64_t tickValue, bool bForceMajorTick) {
            uint64_t   tickMultiplier = tickValue / tickInterval;
            const bool bMajorTick     = (((tickMultiplier) % 10) == 0) || bForceMajorTick;
            const bool bMediumTick    = ((tickMultiplier % 5) == 0);

            const float tickPosXOffset = zoom.UnitsToPixels(tickValue - zoom.GetVisibleRangeBegin());
            const float tickPosX       = rulerTopLeft.x + tickPosXOffset;
            const float tickPosTop     = rulerBottomRight.y - tickLength * (bMajorTick ? 2 : (bMediumTick ? 1.6f : 1));

            pDrawList->AddLine(ImVec2(tickPosX, rulerBottomRight.y - 1), ImVec2(tickPosX, tickPosTop), textColor);

            const bool bSparseMinorTick = ((tickInterval == 1) && (tickIntervalPixels > 50.0f));

            if (bMajorTick || bSparseMinorTick)
            {
                StrBuilder<> sb;
                sb.AppendFormat("%" PRIu64, tickValue);

                const ImVec2 textSize = ImGui::GetFont()->CalcTextSizeA(
                    ImGui::GetFontSize(), FLT_MAX, 0.0f, sb.c_str(), sb.c_str() + sb.Length());
                const float textLeft =
                    rpsClamp(tickPosX - textSize.x * 0.5f, rulerTopLeft.x, rulerBottomRight.x - textSize.x);

                if (bForceMajorTick || (((textLeft + textSize.x) < labelTextCutoffX) && (textLeft > prevLabelRight)))
                {
                    pDrawList->AddText(
                        ImVec2(textLeft, tickPosTop - textSize.y - 2), textColor, sb.c_str(), sb.c_str() + sb.Length());

                    prevLabelRight = textLeft + textSize.x + labelMinSpacing;
                }
            }
        };

        drawTick(zoom.GetVisibleRangeBegin(), true);

        for (uint64_t tick = RoundUpToMultiplesOf(zoom.GetVisibleRangeBegin() + 1, tickInterval);
             tick < zoom.GetVisibleRangeEnd();
             tick += tickInterval)
        {
            drawTick(tick, false);
        }

        drawTick(zoom.GetVisibleRangeEnd(), true);

        ImGui::EndChild();

        ImGui::PopClipRect();

        const float  scrollBarSize        = ImGui::GetStyle().ScrollbarSize;
        const ImVec2 scrollBarTopLeft     = ImVec2(rulerTopLeft.x, state.rightPaneBottomRightScr.y - scrollBarSize);
        const ImVec2 scrollBarBottomRight = ImVec2(rulerTopLeft.x + rulerSize.x, state.rightPaneBottomRightScr.y);
        ImGui::SetCursorScreenPos(scrollBarTopLeft);

        const float virtualRangePixels = m_timelineZoom.GetTotalRangeInPixels();
        ImGui::SetNextWindowContentSize(ImVec2(virtualRangePixels, scrollBarSize));
        ImGui::BeginChild("TimeLineScrollX",
                          ImVec2(rulerSize.x, scrollBarSize),
                          false,
                          ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoBackground);

        // Ctrl + Z : Zoom to selection
        if (m_timelineSelectState.HasSelection() && !ImGui::IsMouseDown(ImGuiMouseButton_Left) && io.KeyCtrl &&
            ImGui::IsKeyPressed(ImGuiKey_Z) && state.settings.bDrawSelector)
        {
            const U64Vec2 selectionRangeUnits = m_timelineSelectState.GetSelectionRangeOrdered();

            m_timelineZoom.ZoomToUnitRange(selectionRangeUnits.x, selectionRangeUnits.y);

            bZooming = true;
        }

        //Horizontal movement
        const int stepsLeft =
            ImGui::GetKeyPressedAmount(ImGuiKey_A, OverlayState::ButtonRepeatDelay, OverlayState::ButtonRepeatRate);
        const int stepsRight =
            ImGui::GetKeyPressedAmount(ImGuiKey_D, OverlayState::ButtonRepeatDelay, OverlayState::ButtonRepeatRate);
        const int stepsTotal = stepsRight - stepsLeft;
        bMoving              = stepsTotal != 0;
        m_timelineZoom.MoveByUnits(stepsTotal * CalcKeyMoveTimeStep(m_timelineZoom.GetVisibleRangeUnits()));

        if (bZooming || bMoving)
        {
            ImGui::SetScrollX(zoom.GetScrollInPixels());
        }
        else if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            const float scroll = ImGui::GetScrollX();
            m_timelineZoom.SetScrollInPixels(scroll);
        }

        const bool bScrollXVisible = (virtualRangePixels > rulerSize.x);
        const bool bMouseOnScrollBar =
            bScrollXVisible && ImGui::IsMouseHoveringRect(scrollBarTopLeft, scrollBarBottomRight, false);

        const ImU32 cursorLineColor = ImGui::GetColorU32(ImGuiCol_Separator);

        ImDrawList* const pTopLevelDrawList = ImGui::GetWindowDrawList();

        if (ImGui::IsMouseHoveringRect(state.rightPaneTopLeftScr, state.rightPaneBottomRightScr, false))
        {
            pTopLevelDrawList->PushClipRect(state.rightPaneTopLeftScr, state.rightPaneBottomRightScr);
            pTopLevelDrawList->AddLine(ImVec2(state.mousePosScr.x, state.rightPaneTopLeftScr.y),
                                       ImVec2(state.mousePosScr.x, state.rightPaneBottomRightScr.y),
                                       cursorLineColor);
            pTopLevelDrawList->PopClipRect();

            const uint64_t timelineHoveringPos =
                m_timelineZoom.Pick(rpsClamp(state.mousePosScr.x - rulerTopLeft.x, 0.0f, rulerSize.x));

            //Uses side effect of SetActiveID to cancel window moves if mouse is clicked over the right pane.
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !bMouseOnScrollBar)
            {
                m_timelineSelectState.BeginDrag(timelineHoveringPos);
                ImGui::SetActiveID(ImGui::GetActiveID(), nullptr);
            }
            else if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f) && m_timelineSelectState.IsDragging())
            {
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
                {
                    const bool draggingRight = (m_timelineSelectState.GetSelectionRange().x <= timelineHoveringPos);
                    m_timelineSelectState.DragTo(timelineHoveringPos + (draggingRight ? 1 : 0));
                }
                ImGui::SetActiveID(ImGui::GetActiveID(), nullptr);
            }
        }

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            m_timelineSelectState.EndDrag();
        }

        const UIRect timelineViewRect = {state.rightPaneTopLeftScr, state.rightPaneBottomRightScr};
        const bool   bHoveringTimelineViewRect =
            ImGui::IsMouseHoveringRect(timelineViewRect.start, timelineViewRect.end, false);

        if (!bHoveringTimelineViewRect && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            m_timelineSelectState.EndDrag();
            m_timelineSelectState.ClearSelection();
        }

        if (m_timelineSelectState.HasSelection() && state.settings.bDrawSelector)
        {
            pTopLevelDrawList->PushClipRect(state.rightPaneTopLeftScr, state.rightPaneBottomRightScr, false);

            const ImU32 selectionOverlayColor = ImGui::GetColorU32(ImGuiCol_TextSelectedBg, 0.4f);

            const U64Vec2 selectedRange    = m_timelineSelectState.GetSelectionRangeOrdered();
            const ImVec2  selectedRangeScr = {
                rulerTopLeft.x + m_timelineZoom.UnitsToPixels(selectedRange.x - zoom.GetVisibleRangeBegin()),
                rulerTopLeft.x + m_timelineZoom.UnitsToPixels(selectedRange.y - zoom.GetVisibleRangeBegin())};

            pTopLevelDrawList->AddRectFilled(
                ImVec2(selectedRangeScr.x, rulerTopLeft.y),
                ImVec2(selectedRangeScr.y, bScrollXVisible ? scrollBarTopLeft.y : state.rightPaneBottomRightScr.y),
                selectionOverlayColor);

            pTopLevelDrawList->PopClipRect();
        }

        ImGui::EndChild();

        const CmdVisualizationInfo* pHoveringOnCmd = PickCmdVisInfoAtMouseCursor(state);

        if (pHoveringOnCmd)
        {
            if (ImGui::IsMouseHoveringRect(rulerTopLeft, rulerBottomRight, false))
            {
                ImGui::BeginTooltip();
                StrBuilder<> sb;
                sb.AppendFormat("[%u] - node : [%u] ", pHoveringOnCmd->timelinePosition, pHoveringOnCmd->cmdId);
                pHoveringOnCmd->name.Print(sb.AsPrinter());
                ImGui::TextUnformatted(sb.c_str());
                ImGui::EndTooltip();
            }
            // Select single cmd range on double click
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                m_timelineSelectState.SetSelectionRange(
                    U64Vec2{pHoveringOnCmd->timelinePosition, pHoveringOnCmd->timelinePosition + 1});
            }
        }
    }

    const CmdVisualizationInfo* OverlayState::PickCmdVisInfoAtMouseCursor(DrawingState& state)
    {
        const auto& visData = state.visData;

        const uint64_t timelinePos = state.timelineZoom.Pick(state.mousePosScr.x - state.rightPaneTopLeftScr.x);

        if (timelinePos < visData.timelinePosToCmdIdMap.size())
        {
            const uint32_t cmdIdx = visData.timelinePosToCmdIdMap[size_t(timelinePos)];
            if ((cmdIdx < visData.cmdVisInfos.size()))
            {
                return &visData.cmdVisInfos[cmdIdx];
            }
        }

        return nullptr;
    }

    uint64_t OverlayState::CalcKeyMoveTimeStep(uint64_t timeUnits)
    {
        constexpr uint64_t HorizontalScrollBase = 500;
        return timeUnits / HorizontalScrollBase + 1;
    }

    void DrawSettings::ForMembers(std::function<bool(const char*, bool&)> func)
    {
        const struct
        {
            const char* memberName;
            bool&       value;
        } memberTable[] = {
            {"bDrawSelector", bDrawSelector},
            {"bDrawResourceAccesses", bDrawResourceAccesses},
            {"bDrawResourceTransitions", bDrawResourceTransitions},
            {"bDrawResourceConnectors", bDrawResourceConnectors},
            {"bDrawResourceTooltips", bDrawResourceTooltips},
            {"bDrawSubResourceDataLifetimeMarkers", bDrawSubResourceDataLifetimeMarkers},
            {"bDrawHeapTooltips", bDrawHeapTooltips},
            {"bDrawGraphTooltips", bDrawGraphTooltips},
        };

        for (const auto& pair : memberTable)
        {
            if (!func(pair.memberName, pair.value))
                break;
        }
    }

}  // namespace rps
