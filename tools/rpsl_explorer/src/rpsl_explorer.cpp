// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#include "rpsl_explorer.hpp"

#if RPS_D3D12_RUNTIME
#include "rpsl_explorer_dx12_renderer.hpp"
#endif

#if RPS_VK_RUNTIME
#include "rpsl_explorer_vk_renderer.hpp"
#endif

#include "rps_imgui_helpers.hpp"

rps::CmdArg<bool>  g_useDX12("dx12", false, {}, false);
rps::CmdArg<bool>  g_useVK("vk", false, {}, false);
rps::CmdArg<bool>  g_enableDiagDump("diag", false);
rps::CmdArg<bool>  g_enableDebugNames("debugNames", true);
rps::CmdArg<bool>  g_enableVisualizer("vis", true);
rps::CmdArg<float> g_visScreenHeightFrac("visHeight", -1.f);

static inline std::filesystem::path GetAppDataFolder()
{
    PWSTR szAppDataPath;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_DEFAULT, NULL, &szAppDataPath)))
    {
        auto configFileDir = std::filesystem::path(szAppDataPath) / "rpsl_explorer";
        CoTaskMemFree(szAppDataPath);

        std::filesystem::create_directories(configFileDir);

        return configFileDir;
    }

    return ".";
}

RpslExplorer::RpslExplorer()
    : m_mt19937(std::random_device()())
{
    if ((g_useVK && !g_useDX12))
    {
#if RPS_VK_RUNTIME
        m_renderer = std::make_unique<ToolVKRenderer>(this);
#endif
    }
    else
    {
#if RPS_D3D12_RUNTIME
        m_renderer = std::make_unique<ToolDX12Renderer>(this);
#endif
    }

    RpsPrinter debugPrinter = {};
    debugPrinter.pContext   = this;
    debugPrinter.pfnPrintf  = &RecordRpsOutput;
    debugPrinter.pfnVPrintf = &RecordRpsOutputV;

    rpsSetGlobalDebugPrinter(&debugPrinter);
}

bool RpslExplorer::Init(void* window)
{
    m_hWnd = HWND(window);

    CreateRpsDevice();

    m_pEnableDX12EnhancedBarriers = rps::CLI::FindCmdArg("-dx12-eb")->AsPtr<bool>();

    m_threadPool.Init(1);
    m_fileMonitor.SetNotificationCallback([this](const std::string& fileName) { OnSourceUpdated(fileName); });

    // Init ImGUI
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplWin32_Init(window);

    const auto folder          = GetAppDataFolder();
    m_imGuiConfigFilePath      = (folder / "imgui.ini").string();
    ImGui::GetIO().IniFilename = m_imGuiConfigFilePath.c_str();

    return true;
}

void RpslExplorer::CreateRpsDevice()
{
    RpsDeviceCreateInfo createInfo = {};
    createInfo.printer.pfnPrintf   = RecordRpsOutput;
    createInfo.printer.pContext    = this;

    if (m_renderer)
    {
        ThrowIfFailedRps(m_renderer->CreateRpsRuntimeDevice(createInfo, m_hRpsDevice));
    }
    else
    {
        ThrowIfFailedRps(rpsDeviceCreate(&createInfo, &m_hRpsDevice));
    }

    const auto folder = GetAppDataFolder();

    const std::string             pathStr       = folder.string();
    const RpsVisualizerCreateInfo visCreateInfo = {RPS_VISUALIZER_CREATE_CHILD_WINDOW_BIT, (pathStr.c_str())};
    ThrowIfFailedRps(rpsVisualizerCreate(m_hRpsDevice, &visCreateInfo, &m_hRpsVisualizer));
}

void RpslExplorer::UpdateRpsPipeline(uint64_t                         frameIndex,
                                     uint64_t                         completedFrameIndex,
                                     uint32_t                         numArgs,
                                     const RpsConstant*               argData,
                                     const RpsRuntimeResource* const* argResources)
{
    if ((m_hRenderGraph != RPS_NULL_HANDLE) && m_renderGraphUpdateEnabled)
    {
        assert(numArgs <= m_entryArgPtrs.size());

        RpsRenderGraphUpdateInfo updateInfo = {};
        updateInfo.frameIndex               = frameIndex;
        updateInfo.gpuCompletedFrameIndex   = completedFrameIndex;
        updateInfo.scheduleFlags            = m_scheduleFlags;

        // Only actually needed if RPS_SCHEDULE_RANDOM_ORDER_BIT is set.
        RpsRandomNumberGenerator randGen  = {};
        randGen.pContext                  = this;
        randGen.pfnRandomUniformInt       = &RandGenCallback;
        updateInfo.pRandomNumberGenerator = &randGen;

        for (uint32_t i = 0; i < numArgs; i++)
        {
            m_entryArgPtrs[i] = argData[i];
        }

        updateInfo.numArgs        = uint32_t(m_entryArgPtrs.size());
        updateInfo.ppArgs         = m_entryArgPtrs.data();
        updateInfo.ppArgResources = argResources;

        if (g_enableDiagDump &&
            ((completedFrameIndex == RPS_GPU_COMPLETED_FRAME_INDEX_NONE) || (m_renderGraphUpdateCount < 1)))
        {
            updateInfo.diagnosticFlags |= RPS_DIAGNOSTIC_ENABLE_PRE_SCHEDULE_DUMP |
                                          RPS_DIAGNOSTIC_ENABLE_POST_SCHEDULE_DUMP | RPS_DIAGNOSTIC_ENABLE_DAG_DUMP;
        }

        if (g_enableDebugNames)
        {
            updateInfo.diagnosticFlags |= RPS_DIAGNOSTIC_ENABLE_RUNTIME_DEBUG_NAMES;
        }

        RpsResult result         = RPS_OK;
        int64_t   currUpdateTime = 0;

        do
        {
            RpsAfxScopedCpuTimer timer(nullptr, &currUpdateTime);
            result = rpsRenderGraphUpdate(m_hRenderGraph, &updateInfo);
        } while (false);

        if (RPS_SUCCEEDED(result))
        {
            m_renderGraphUpdateTimeSampler.Update(currUpdateTime);
        }
        else
        {
            LogFmt("\nUpdate RenderGraph: Failed (0x%x)", result);

            m_renderGraphUpdateEnabled = false;
        }

        if (RPS_SUCCEEDED(result) && (m_bPendingVisualizerUpdate || (m_renderGraphUpdateCount == 0)))
        {
            RpsVisualizerUpdateInfo visUpdateInfo = {};
            visUpdateInfo.hRenderGraph            = m_hRenderGraph;

            result = rpsVisualizerUpdate(m_hRpsVisualizer, &visUpdateInfo);

            if (RPS_FAILED(result))
            {
                LogFmt("\nVisualizer Update failed: error code (0x%x).", result);
            }

            m_bPendingVisualizerUpdate = false;
        }

        m_renderGraphUpdateCount++;
    }
}

void RpslExplorer::RenderImGuiFrame()
{
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Open"))
            {
                OpenRPSLFile();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    RECT rect;
    GetWindowRect(m_hWnd, &rect);
    ImGui::SetNextWindowSize(
        {static_cast<float>(rect.right - rect.left) / 2, static_cast<float>(rect.bottom - rect.top) / 2},
        ImGuiCond_FirstUseEver);

    ImGui::Begin("Overlay", nullptr, ImGuiWindowFlags_NoScrollbar);

    if (g_visScreenHeightFrac <= 0.0f)
    {
        g_visScreenHeightFrac = DefaultVisHeightFrac;
    }

    if (ImGui::BeginTable("Output Types",
                          2,
                          ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
                          {0.0f, g_visScreenHeightFrac * ImGui::GetWindowHeight()}))
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        ImGui::BeginChild(
            "Control Panel## Child", {0, -ImGui::GetStyle().FramePadding.y}, false, ImGuiWindowFlags_NoScrollbar);

        rps::custom_imgui::ChildWindowTitleBar("Control Panel");
        ImGui::Text("Module : %s", m_currentModule.moduleName.empty() ? "<none>" : m_currentModule.moduleName.c_str());

        int currEntry = int(m_selectedEntryPointId);
        ImGui::Text("Entry  :");
        ImGui::SameLine();
        bool bReloadRenderGraph =
            ((ImGui::Combo("##_EntryCombo", &currEntry, EntryNameComboGetter, this, int(m_entryPointNames.size()))) &&
             (m_selectedEntryPointId != currEntry));

        ImGui::Text("RenderGraph Update CPU time: %" PRId64 " us",
                    m_renderGraphUpdateEnabled ? m_renderGraphUpdateTimeSampler.m_avg : 0);

        ImGui::Text("Visualizer Draw CPU Time: %" PRId64 " us", m_frameTimeSampler.m_avg);

        ImGui::Spacing();

        ImGui::Checkbox("Enable Diagnostic Dump", &g_enableDiagDump);
        ImGui::Checkbox("Show Visualizer", &g_enableVisualizer);
        ImGui::Checkbox("Update Visualizer per frame", &m_bUpdateVisualizerPerFrame);
        m_bPendingVisualizerUpdate |= m_bUpdateVisualizerPerFrame;

        bool bRecreateRpsDevice = false;

        if (m_pEnableDX12EnhancedBarriers)
        {
            bRecreateRpsDevice |= ImGui::Checkbox("Enable DX12 Enhanced Barriers", m_pEnableDX12EnhancedBarriers);
        }
        bReloadRenderGraph |= ImGui::Checkbox("Enable Aliasing", &m_enableAliasing);

        // clang-format off
#define RPSL_EXPLORER_DEFINE_SCH_OPT(Name) {#Name, RPS_##Name##_BIT}
    static const struct
    {
        const char*         name;
        RpsScheduleFlagBits flagBit;
    } scheduleOpts[] = {
        RPSL_EXPLORER_DEFINE_SCH_OPT(SCHEDULE_KEEP_PROGRAM_ORDER),
        RPSL_EXPLORER_DEFINE_SCH_OPT(SCHEDULE_PREFER_MEMORY_SAVING),
        RPSL_EXPLORER_DEFINE_SCH_OPT(SCHEDULE_RANDOM_ORDER),
        RPSL_EXPLORER_DEFINE_SCH_OPT(SCHEDULE_MINIMIZE_COMPUTE_GFX_SWITCH),
        RPSL_EXPLORER_DEFINE_SCH_OPT(SCHEDULE_DISABLE_DEAD_CODE_ELIMINATION),
        RPSL_EXPLORER_DEFINE_SCH_OPT(SCHEDULE_WORKLOAD_TYPE_PIPELINING_DISABLE),
        RPSL_EXPLORER_DEFINE_SCH_OPT(SCHEDULE_WORKLOAD_TYPE_PIPELINING_AGGRESSIVE),
    };
#undef RPSL_EXPLORER_DEFINE_SCH_OPT
        // clang-format on

        for (uint32_t iSchOpts = 0; iSchOpts < _countof(scheduleOpts); iSchOpts++)
        {
            m_bPendingVisualizerUpdate |=
                ImGui::CheckboxFlags(scheduleOpts[iSchOpts].name, &m_scheduleFlags, scheduleOpts[iSchOpts].flagBit);
        }

        if (bRecreateRpsDevice)
        {
            RecreateRpsDevice();
        }

        if (bReloadRenderGraph)
        {
            if (RPS_NULL_HANDLE !=
                TryLoadEntry(m_currentModule.hRpslJITModule, m_currentModule.moduleName, m_entryPointNames[currEntry]))
            {
                m_selectedEntryPointId = uint32_t(currEntry);
            }
        }

        ImGui::Spacing();

        auto getArgsDW = [&](const ArgInfo& argInfo, uint32_t iElem, uint32_t iDW) -> uint32_t {
            size_t         byteOffsetInElem = iDW * sizeof(uint32_t);
            const uint8_t* pElem =
                m_entryArgsBuffer.data() + argInfo.offset + (iElem * argInfo.bytesPerElement) + byteOffsetInElem;

            assert(argInfo.bytesPerElement > byteOffsetInElem);
            auto remainSize = (argInfo.bytesPerElement - byteOffsetInElem) % 4;

            switch (remainSize)
            {
            case 0:
                return *reinterpret_cast<const uint32_t*>(pElem);
            case 2:
                return *reinterpret_cast<const uint16_t*>(pElem);
            case 1:
                return *reinterpret_cast<const uint8_t*>(pElem);
            default:
                assert(false);
                break;
            }
            return 0;
        };

        auto setArgsDW = [&](const ArgInfo& argInfo, uint32_t iElem, uint32_t iDW, uint32_t value) {
            size_t   byteOffsetInElem = iDW * sizeof(uint32_t);
            uint8_t* pElem =
                m_entryArgsBuffer.data() + argInfo.offset + (iElem * argInfo.bytesPerElement) + byteOffsetInElem;

            assert(argInfo.bytesPerElement > byteOffsetInElem);
            auto remainSize = (argInfo.bytesPerElement - byteOffsetInElem) % 4;

            switch (remainSize)
            {
            case 0:
                *reinterpret_cast<uint32_t*>(pElem) = value;
                break;
            case 2:
                *reinterpret_cast<uint16_t*>(pElem) = value;
                break;
            case 1:
                *reinterpret_cast<uint8_t*>(pElem) = value;
                break;
            default:
                assert(false);
                break;
            }
        };

        if (m_hRenderGraph != RPS_NULL_HANDLE)
        {
            ImGui::CollapsingHeader("Arguments", ImGuiTreeNodeFlags_DefaultOpen);

            char paramValueBuf[64];

            for (uint32_t iParam = 0; iParam < m_currRpslEntryDesc.numParams; iParam++)
            {
                auto& paramDesc = m_currRpslEntryDesc.pParamDescs[iParam];
                auto& argInfo   = m_entryArgsInfo[iParam];

                if (paramDesc.flags & RPS_PARAMETER_FLAG_RESOURCE_BIT)
                    continue;

                if (ImGui::TreeNode(paramDesc.name))
                {
                    ImGui::BeginTable((std::string("##tbl") + paramDesc.name).c_str(), 4);

                    for (uint32_t iElem = 0; iElem < argInfo.numElements; iElem++)
                    {
                        ImGui::TableNextRow();
                        for (uint32_t iDW = 0; iDW < argInfo.numDWPerElement; iDW++)
                        {
                            uint32_t value = getArgsDW(argInfo, iElem, iDW);
                            snprintf(paramValueBuf, _countof(paramValueBuf), "0X%X", value);

                            ImGui::TableNextColumn();

                            ImGui::SetNextItemWidth(96);
                            if (ImGui::InputText(
                                    ("##param_" + std::to_string(iParam) + "_" + std::to_string(iDW)).c_str(),
                                    paramValueBuf,
                                    _countof(paramValueBuf),
                                    ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase))
                            {
                                size_t pos;
                                try
                                {
                                    auto newValue = std::stoul(paramValueBuf, &pos, 0);
                                    if (pos == strlen(paramValueBuf))
                                    {
                                        if (newValue != value)
                                        {
                                            setArgsDW(argInfo, iElem, iDW, newValue);

                                            m_renderGraphUpdateCount   = 0;
                                            m_renderGraphUpdateEnabled = true;
                                        }
                                    }
                                }
                                catch (const std::invalid_argument&)
                                {
                                }
                                catch (const std::out_of_range&)
                                {
                                }
                            }
                        }
                    }

                    ImGui::EndTable();

                    ImGui::TreePop();
                }
            }
        }

        ImGui::EndChild();

        ImGui::TableNextColumn();

        rps::custom_imgui::ChildWindowTitleBar("Visualizer");

        int64_t visualizerDrawTime = 0;

        if (g_enableVisualizer)
        {
            RpsAfxScopedCpuTimer timer(nullptr, &visualizerDrawTime);
            RpsResult result = rpsVisualizerDrawImGui(m_hRpsVisualizer);

            if (RPS_FAILED(result))
            {
                LogFmt("\nVisualizer Draw failed: error code (0x%x).", result);
            }
        }

        m_frameTimeSampler.Update(visualizerDrawTime);

        ImGui::EndTable();
    }

    constexpr float minSplitterY            = 1.0f;
    constexpr float maxVisualizerHeightFrac = 0.875f;

    float fracContainer = g_visScreenHeightFrac * ImGui::GetWindowHeight();
    rps::custom_imgui::DrawHorizontalSplitter(
        "##Splitter", fracContainer, minSplitterY, maxVisualizerHeightFrac * ImGui::GetWindowHeight());
    g_visScreenHeightFrac = fracContainer / ImGui::GetWindowHeight();

    rps::custom_imgui::ChildWindowTitleBar("Text Output");
    ImGui::BeginChild("Text Output## Child");

    const float beginHeight = ImGui::GetCursorScreenPos().y;

    do
    {
        std::scoped_lock<std::mutex> logLock(m_logMutex);
        ImGui::TextUnformatted(m_outputBuf.c_str());
    } while (false);

    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.0f);

    ImGui::EndChild();

    ImGui::End();

    ImGui::Render();
}

INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow)
{
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#if defined(BREAK_AT_ALLOC_ID)
    _CrtSetBreakAlloc(BREAK_AT_ALLOC_ID);
#endif

    const std::string configFileName = (GetAppDataFolder() / "rpsl_explorer_config.ini").string();

    rps::CLI::LoadConfig(configFileName);
    rps::CLI::Parse(__argc, __argv);

    RpslExplorer tool;

    RpsAfxRunWindowInfo runInfo = {};
    runInfo.title               = TEXT("RPSL Explorer");
    runInfo.width               = 1280;
    runInfo.height              = 720;
    runInfo.pRenderer           = tool.GetRenderer();

    RpsAfxRunWindowApp(&runInfo);

    rps::CLI::SaveConfig(configFileName);

    return 0;
}
