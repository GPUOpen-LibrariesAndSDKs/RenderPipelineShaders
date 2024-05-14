// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#pragma once

#define NOMINMAX
#define VK_USE_PLATFORM_WIN32_KHR
#define USE_RPSL_JIT 1

#include "rps/rps.h"

#include "rps_visualizer/include/rps_visualizer.h"

#include "app_framework/afx_common_helpers.hpp"
#include "app_framework/afx_threadpool.hpp"
#include "app_framework/afx_win32.hpp"
#include "app_framework/afx_cmd_parser.hpp"

#include "file_monitor.hpp"

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_win32.h"

#include <sstream>
#include <filesystem>
#include <codecvt>
#include <locale>
#include <thread>
#include <mutex>
#include <random>

void ThrowIfFailedRps(RpsResult result)
{
    if (RPS_FAILED(result))
        throw result;
}

static constexpr uint32_t UM_RPSL_MODULE_UPDATED = WM_USER + 4098;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

class RpslExplorer
{
public:
    RpslExplorer();

    bool Init(void* window);

    void Tick()
    {
    }

    void UpdateRpsPipeline(uint64_t                         frameIndex,
                           uint64_t                         completedFrameIndex,
                           uint32_t                         numArgs,
                           const RpsConstant*               argData,
                           const RpsRuntimeResource* const* argResources);

    void RenderImGuiFrame();

    void CleanUp()
    {
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        DestroyRpsDevice();
    }

    void OnResize(uint32_t width, uint32_t height)
    {
    }

    LRESULT WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, bool& bHandled)
    {
        if ((bHandled = m_fileMonitor.HandleMessage(message, wParam, lParam)))
            return 0;

        if ((bHandled = ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam)))
            return 1;

        switch (message)
        {
        case UM_RPSL_MODULE_UPDATED:
            bHandled = true;
            HandleModuleUpdate();
            return 0;
        }

        bHandled = false;
        return 0;
    }

    RpsAfxRendererBase* GetRenderer() const
    {
        return m_renderer.get();
    }

    RpsRenderGraph GetRenderGraph() const
    {
        return m_renderGraphUpdateEnabled ? m_hRenderGraph : RPS_NULL_HANDLE;
    }

public:
    static constexpr float DefaultVisHeightFrac = 0.75f;

private:
    void CreateRpsDevice();

    void DestroyRpsDevice()
    {
        rpsRenderGraphDestroy(m_hRenderGraph);
        rpsVisualizerDestroy(m_hRpsVisualizer);
        rpsDeviceDestroy(m_hRpsDevice);

        m_hRenderGraph   = RPS_NULL_HANDLE;
        m_hRpsVisualizer = RPS_NULL_HANDLE;
        m_hRpsDevice     = RPS_NULL_HANDLE;
    }

    void RecreateRpsDevice()
    {
        if (m_renderer)
        {
            m_renderer->WaitForGpuIdle();
        }

        const bool bReloadRenderGraph = (m_hRenderGraph != RPS_NULL_HANDLE);

        DestroyRpsDevice();
        CreateRpsDevice();

        if (bReloadRenderGraph && (m_currentModule.hRpslJITModule != RPS_NULL_HANDLE) &&
            (m_entryPointNames.size() > m_selectedEntryPointId) && !m_entryPointNames[m_selectedEntryPointId].empty())
        {
            TryLoadEntry(
                m_currentModule.hRpslJITModule, m_currentModule.moduleName, m_entryPointNames[m_selectedEntryPointId]);
        }
    }

    void OpenRPSLFile()
    {
        OPENFILENAMEA ofn;
        CHAR          szFile[260];

        // Initialize OPENFILENAME
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner   = m_hWnd;
        ofn.lpstrFile   = szFile;
        // Set lpstrFile[0] to '\0' so that GetOpenFileName does not
        // use the contents of szFile to initialize itself.
        ofn.lpstrFile[0]    = '\0';
        ofn.nMaxFile        = sizeof(szFile);
        ofn.lpstrFilter     = "RPSL File\0*.rpsl\0";
        ofn.nFilterIndex    = 1;
        ofn.lpstrFileTitle  = NULL;
        ofn.nMaxFileTitle   = 0;
        ofn.lpstrInitialDir = NULL;
        ofn.Flags           = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

        // Display the Open dialog box.
        if (GetOpenFileNameA(&ofn) == TRUE)
        {
            if (!m_fileMonitor.BeginWatch(m_hWnd, szFile))
            {
                LogFmt("\nFailed to set up monitoring for file changes!");
            }
            // TODO: Watch includes too

            OnSourceUpdated(szFile);
        }
    }

    void OnSourceUpdated(const std::string& fileName)
    {
        if (fileName.empty())
            return;

        do
        {
            std::scoped_lock<std::mutex> pendingFileLock(m_pendingFileMutex);
            m_pendingFileName = fileName;
        } while (false);

        m_threadPool.EnqueueJob([this]() {
            std::string pendingFileName;
            do
            {
                std::scoped_lock<std::mutex> pendingFileLock(m_pendingFileMutex);
                std::swap(pendingFileName, m_pendingFileName);
            } while (false);

            if (pendingFileName.empty())
                return;

            std::scoped_lock<std::mutex> jitLock(m_jitMutex);

            LogFmt("\nTrying to load RPSL '%s'...", pendingFileName.c_str());

            auto rpslFilePath = std::filesystem::path(pendingFileName);
            auto moduleName   = rpslFilePath.stem().string();
            auto tmpDir       = rpslFilePath.parent_path() / "tmp";

            std::replace_if(
                moduleName.begin(),
                moduleName.end(),
                [](auto c) { return !(isalpha(c) || isdigit(c) || ('_' == c)); },
                '_');

            std::filesystem::create_directories(tmpDir);

            auto bitCodeFile = tmpDir / (moduleName + ".llvm.bc");

            // TODO: Make dxcompiler.dll to compile RPSL directly
            // Call rps-hlslc, compile string to bitcode

            if (!std::filesystem::exists(bitCodeFile) ||
                (std::filesystem::last_write_time(bitCodeFile) < std::filesystem::last_write_time(rpslFilePath)))
            {
                LogFmt("\nCompiling...");

                auto compilerPath = std::filesystem::path("rps_hlslc/rps-hlslc.exe");
                compilerPath.make_preferred();

                std::stringstream rpsHlslcCmdLine;
                rpsHlslcCmdLine << compilerPath << " " << rpslFilePath << " -od " << tmpDir << " -m " << moduleName
                                << " -O3 -rps-target-dll -rps-bc -cbe=0";


                std::string cmdStr = rpsHlslcCmdLine.str();

                if (!LaunchProcess(cmdStr.c_str()))
                {
                    LogFmt("\nFailed to compile RPSL '%s'", cmdStr.c_str());
                    return;
                }

                LogFmt("OK.");
            }
            else
            {
                LogFmt("\nFound cached bitcode, skipping compilation.");
            }

            LogFmt("\nLoading JIT module...");

            int64_t      jitTime    = 0;
            RpsJITModule hJITModule = m_JITHelper.LoadBitcode(bitCodeFile.string().c_str(), &jitTime);

            LogFmt("(%.3f ms)", jitTime / 1000.0f);

            const char* const* entryNames = m_JITHelper.GetEntryNameTable(hJITModule);
            if ((entryNames == nullptr) || (entryNames[0] == nullptr))
            {
                m_JITHelper.pfnRpsJITUnload(hJITModule);
                return;
            }

            std::string newModuleName = m_JITHelper.GetModuleName(hJITModule);
            assert(newModuleName == moduleName);

            do
            {
                std::scoped_lock<std::mutex> pendingModuleLock(m_pendingStateMutex);

                m_pendingModule.fileName       = pendingFileName;
                m_pendingModule.hRpslJITModule = hJITModule;
                m_pendingModule.moduleName     = newModuleName;

            } while (false);

            PostMessage(m_hWnd, UM_RPSL_MODULE_UPDATED, 0, 0);
        });
    }

    void HandleModuleUpdate()
    {
        uint32_t selectedEntryPoint = 0;
        uint32_t numEntryPoints     = 0;

        JITModule pendingModule;
        do
        {
            std::scoped_lock<std::mutex> pendingModuleLock(m_pendingStateMutex);
            pendingModule = std::move(m_pendingModule);
        } while (false);

        const char* const* entryNames = m_JITHelper.GetEntryNameTable(pendingModule.hRpslJITModule);

        auto prevEntryName =
            (m_entryPointNames.size() > m_selectedEntryPointId) ? m_entryPointNames[m_selectedEntryPointId] : "";

        while (entryNames[numEntryPoints])
        {
            if (!prevEntryName.empty() && (prevEntryName == entryNames[numEntryPoints]))
            {
                selectedEntryPoint = numEntryPoints;
            }

            numEntryPoints++;
        }

        LogFmt("\nFound %d entry point%s.", numEntryPoints, numEntryPoints > 1 ? "s" : "");

        if (entryNames[selectedEntryPoint])
        {
            LogFmt("\nTry loading entry point %d '%s'...", selectedEntryPoint, entryNames[selectedEntryPoint]);

            RpsRenderGraph hRenderGraph =
                TryLoadEntry(pendingModule.hRpslJITModule, pendingModule.moduleName, entryNames[selectedEntryPoint]);

            if (hRenderGraph != RPS_NULL_HANDLE)
            {
                m_selectedEntryPointId = selectedEntryPoint;

                if (m_currentModule.hRpslJITModule)
                {
                    m_JITHelper.pfnRpsJITUnload(m_currentModule.hRpslJITModule);
                    m_currentModule.hRpslJITModule = RPS_NULL_HANDLE;
                }

                m_currentModule = std::move(pendingModule);

                m_entryPointNames.clear();
                for (const char* const* pEntryName = entryNames; *pEntryName != 0; pEntryName++)
                {
                    m_entryPointNames.push_back(*pEntryName);
                }
            }
        }
    }

    RpsRenderGraph TryLoadEntry(RpsJITModule hJITModule, const std::string& moduleName, const std::string& entryName)
    {
        char         nameBuf[256];
        RpsRpslEntry hEntry = m_JITHelper.GetEntryPoint(
            hJITModule, rpsMakeRpslEntryName(nameBuf, std::size(nameBuf), moduleName.c_str(), entryName.c_str()));

        RpsRenderGraph           hRenderGraph                     = {};
        RpsRenderGraphCreateInfo renderGraphCreateInfo            = {};
        renderGraphCreateInfo.mainEntryCreateInfo.hRpslEntryPoint = hEntry;

        renderGraphCreateInfo.renderGraphFlags |=
            (m_enableAliasing ? RPS_RENDER_GRAPH_FLAG_NONE : RPS_RENDER_GRAPH_NO_GPU_MEMORY_ALIASING);

        RpsResult result = rpsRenderGraphCreate(m_hRpsDevice, &renderGraphCreateInfo, &hRenderGraph);
        LogFmt("\nCreate RenderGraph: %s (0x%x)", RPS_SUCCEEDED(result) ? "OK" : "Failed", result);

        if (RPS_SUCCEEDED(result) && (hRenderGraph != RPS_NULL_HANDLE))
        {
            if (m_hRenderGraph != RPS_NULL_HANDLE)
            {
                rpsVisualizerUpdate(m_hRpsVisualizer, nullptr);

                m_renderer->WaitForGpuIdle();
                rpsRenderGraphDestroy(m_hRenderGraph);
            }

            if (RPS_FAILED(rpsRpslEntryGetSignatureDesc(hEntry, &m_currRpslEntryDesc)))
            {
                LogFmt("Failed to reflect RPSL.");
                m_currRpslEntryDesc      = {};
                m_currRpslEntryDesc.name = "<error>";
            }
            else
            {
                size_t argBufferSize = 0;
                m_entryArgsInfo.resize(m_currRpslEntryDesc.numParams);

                for (uint32_t iParam = 0; iParam < m_currRpslEntryDesc.numParams; iParam++)
                {
                    const auto& paramDesc = m_currRpslEntryDesc.pParamDescs[iParam];
                    auto&       argInfo   = m_entryArgsInfo[iParam];

                    argInfo.numElements     = (paramDesc.arraySize ? paramDesc.arraySize : 1);
                    argInfo.bytesPerElement = paramDesc.typeInfo.size;
                    argInfo.numDWPerElement = DivRoundUp(paramDesc.typeInfo.size, sizeof(uint32_t));
                    argInfo.offset          = uint32_t(argBufferSize);

                    const size_t paramSize =
                        AlignUp<size_t>(argInfo.bytesPerElement * argInfo.numElements, sizeof(uint32_t));
                    argBufferSize += paramSize;
                }

                m_entryArgsBuffer.resize(argBufferSize);
                m_entryArgPtrs.resize(m_currRpslEntryDesc.numParams, nullptr);

                for (uint32_t iParam = 0; iParam < m_currRpslEntryDesc.numParams; iParam++)
                {
                    m_entryArgPtrs[iParam] = &m_entryArgsBuffer[m_entryArgsInfo[iParam].offset];
                }
            }

            m_hRenderGraph             = hRenderGraph;
            m_renderGraphUpdateEnabled = true;
            m_renderGraphUpdateCount   = 0;
        }

        return hRenderGraph;
    }

    static void DefaultNodeCallback(const RpsCmdCallbackContext* pContext)
    {
    }

    static bool EntryNameComboGetter(void* data, int idx, const char** out_text)
    {
        RpslExplorer* pThis = static_cast<RpslExplorer*>(data);
        if (size_t(idx) < pThis->m_entryPointNames.size())
        {
            *out_text = pThis->m_entryPointNames[idx].c_str();
            return true;
        }

        return false;
    }

    static int32_t RandGenCallback(void* pUserContext, int32_t minValue, int32_t maxValue)
    {
        auto pThis = static_cast<RpslExplorer*>(pUserContext);
        return std::uniform_int_distribution<>(minValue, maxValue)(pThis->m_mt19937);
    }

private:
    static void RecordRpsOutput(void* pUserContext, const char* format, ...)
    {
        va_list vl;
        va_start(vl, format);
        RecordRpsOutputV(pUserContext, format, vl);
        va_end(vl);
    }

    static void RecordRpsOutputV(void* pUserContext, const char* format, va_list vl)
    {
        char buf[1024];
        std::vsnprintf(buf, _countof(buf), format, vl);

        static_cast<RpslExplorer*>(pUserContext)->Log(buf);
    }

    template <typename... TArgs>
    void LogFmt(const char* fmt, TArgs... args)
    {
        char buf[1024];

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-security"
#endif
        std::snprintf(buf, _countof(buf), fmt, args...);
#ifdef __clang__
#pragma clang diagnostic pop
#endif

        Log(buf);
    }

    void Log(const char* buf)
    {
        std::scoped_lock<std::mutex> logLock(m_logMutex);
        m_outputBuf.append(buf);
        OutputDebugStringA(buf);
    }

private:
    HWND                                m_hWnd = {};
    FileMonitor                         m_fileMonitor;
    std::unique_ptr<RpsAfxRendererBase> m_renderer;

    RpsAfxThreadPool m_threadPool;
    std::string      m_imGuiConfigFilePath;

    std::mutex  m_jitMutex;
    std::mutex  m_logMutex;
    std::mutex  m_pendingStateMutex;
    std::mutex  m_pendingFileMutex;
    std::string m_outputBuf;  // TODO

    RpsDevice       m_hRpsDevice = RPS_NULL_HANDLE;
    RpsAfxJITHelper m_JITHelper;

    struct JITModule
    {
        std::string  moduleName;
        std::string  fileName;
        RpsJITModule hRpslJITModule = RPS_NULL_HANDLE;
    };

    std::string m_pendingFileName;
    JITModule   m_pendingModule;
    JITModule   m_currentModule;

    std::vector<std::string> m_entryPointNames;

    RpsRenderGraph m_hRenderGraph         = RPS_NULL_HANDLE;
    uint32_t       m_selectedEntryPointId = 0;

    std::mt19937 m_mt19937;

    RpsScheduleFlags m_scheduleFlags  = RPS_SCHEDULE_UNSPECIFIED;
    bool             m_enableAliasing = true;

    bool* m_pEnableDX12EnhancedBarriers = nullptr;

    RpsVisualizer m_hRpsVisualizer = RPS_NULL_HANDLE;

    RpsRenderGraphSignatureDesc m_currRpslEntryDesc = {};
    std::vector<uint8_t>        m_entryArgsBuffer   = {};
    struct ArgInfo
    {
        uint32_t offset;
        uint32_t numElements;
        uint32_t bytesPerElement;
        uint32_t numDWPerElement;
    };
    std::vector<ArgInfo>     m_entryArgsInfo;
    std::vector<RpsConstant> m_entryArgPtrs;

    RpsAfxThreadPool::WaitHandle m_pendingModuleLoadingJob;

    RpsAfxAveragedSampler<> m_frameTimeSampler;
    RpsAfxAveragedSampler<> m_renderGraphUpdateTimeSampler;

    bool     m_renderGraphUpdateEnabled   = false;
    bool     m_bPendingVisualizerUpdate   = false;
    bool     m_bUpdateVisualizerPerFrame  = false;
    uint64_t m_renderGraphUpdateCount     = 0;
};
