// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#include "runtime/common/rps_subprogram.hpp"
#include "runtime/common/rps_runtime_device.hpp"
#include "runtime/common/rps_rpsl_host.hpp"

namespace rps
{
    RpsResult Subprogram::Create(const Device& device, const RpsProgramCreateInfo* pCreateInfo, Subprogram** ppInstance)
    {
        RPS_CHECK_ARGS(pCreateInfo);
        RPS_CHECK_ARGS(ppInstance);

        auto  allocInfo = AllocInfo::FromType<Subprogram>();
        void* pMemory   = Allocate(device.Allocator(), allocInfo);

        RPS_CHECK_ALLOC(pMemory);
        auto pInstance = new (pMemory)
            Subprogram(device, FromHandle(pCreateInfo->hRpslEntryPoint), pCreateInfo->defaultNodeCallback);

        *ppInstance = pInstance;

        return pInstance->Init(pCreateInfo);
    }

    RpsResult Subprogram::Init(const RpsProgramCreateInfo* pCreateInfo)
    {
        RPS_RETURN_ERROR_IF(m_pSignature != nullptr, RPS_ERROR_INVALID_OPERATION);

        auto pSignatureDesc = pCreateInfo->pSignatureDesc;

        RpsRenderGraphSignatureDesc signatureDescTmp;
        if (pCreateInfo->hRpslEntryPoint)
        {
            RPS_V_RETURN(rpsRpslEntryGetSignatureDesc(pCreateInfo->hRpslEntryPoint, &signatureDescTmp));
            pSignatureDesc = &signatureDescTmp;
        }

        RenderGraphSignature* pSignature = nullptr;

        if (pSignatureDesc)
        {
            RPS_V_RETURN(RenderGraphSignature::Create(m_arena, pSignatureDesc, &pSignature));
            m_pSignature = pSignature;
            m_nodeImpls  = m_arena.NewArray<RpslNodeImpl>(pSignature->GetNodeDecls().size());

            auto pRuntimeDevice = RuntimeDevice::Get(m_device);
            if (pRuntimeDevice)
            {
                // TODO: Only bind built-in at top level
                auto builtInNodes = pRuntimeDevice->GetBuiltInNodes();
                for (auto& nodeInfo : builtInNodes)
                {
                    auto nodeDeclId = m_pSignature->FindNodeDeclIndexByName(nodeInfo.name);
                    if (nodeDeclId != RPS_INDEX_NONE_U32)
                    {
                        RPS_V_RETURN(Bind(nodeDeclId, nodeInfo.callbackInfo));
                    }
                }
            }
        }

        return RPS_OK;
    }

}  // namespace rps

RpsResult rpsProgramCreate(RpsDevice hDevice, const RpsProgramCreateInfo* pCreateInfo, RpsSubprogram* phRpslInstance)
{
    RPS_CHECK_ARGS(hDevice);
    RPS_CHECK_ARGS(pCreateInfo);
    RPS_CHECK_ARGS(phRpslInstance);

    auto pDevice = rps::FromHandle(hDevice);

    return rps::Subprogram::Create(*pDevice, pCreateInfo, rps::FromHandle(phRpslInstance));
}

void rpsProgramDestroy(RpsSubprogram hRpslInstance)
{
    if (hRpslInstance)
    {
        rps::FromHandle(hRpslInstance)->Destroy();
    }
}

RpsResult rpsProgramBindNodeCallback(RpsSubprogram hRpslInstance, const char* name, const RpsCmdCallback* pCallback)
{
    RPS_CHECK_ARGS(hRpslInstance);

    return rps::FromHandle(hRpslInstance)->Bind(name, pCallback ? *pCallback : RpsCmdCallback{});
}

RpsResult rpsProgramBindNodeSubprogram(RpsSubprogram hRpslInstance, const char* name, RpsSubprogram hSubprogram)
{
    RPS_CHECK_ARGS(hRpslInstance);

    return rps::FromHandle(hRpslInstance)->Bind(name, rps::FromHandle(hSubprogram));
}
