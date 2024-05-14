// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_SUBPROGRAM_HPP
#define RPS_SUBPROGRAM_HPP

#include "runtime/common/rps_render_graph_signature.hpp"

namespace rps
{
    class RenderGraph;
    class ProgramInstance;
    class RpslHost;

    struct RpslEntry
    {
        const char*             name;
        PFN_RpslEntry           pfnEntry;
        const RpsParameterDesc* pParamDescs;
        const RpsNodeDesc*      pNodeDecls;
        uint32_t                numParams;
        uint32_t                numNodeDecls;
    };

    RPS_ASSOCIATE_HANDLE(RpslEntry);

    class Subprogram
    {
        RPS_CLASS_NO_COPY_MOVE(Subprogram);

    public:
        struct RpslNodeImpl
        {
            enum class Type
            {
                Unknown,
                RpslEntry,
                Callback,
            };

            union
            {
                Subprogram*    pSubprogram;
                RpsCmdCallback callback;
            };

            void*    pBuffer    = nullptr;
            uint32_t bufferSize = 0;

            Type type = Type::Unknown;

            RpslNodeImpl() = default;

            void Set(const RpsCmdCallback& inCallback)
            {
                callback = inCallback;
                type     = Type::Callback;
            }

            void Set(Subprogram* pSubprogramIn)
            {
                pSubprogram = pSubprogramIn;
                type        = Type::RpslEntry;
            }

        private:
            RPS_CLASS_NO_COPY_MOVE(RpslNodeImpl);
        };

    private:
        Subprogram(const Device& device, const RpslEntry* pRpslEntry, const RpsCmdCallback& defaultCmdCallback)
            : m_device(device)
            , m_arena(device.Allocator())
            , m_pEntry(pRpslEntry)
        {
            m_defaultNodeImpl.Set(defaultCmdCallback);
        }

        ~Subprogram()
        {
        }

    public:
        static RpsResult Create(const Device& device, const RpsProgramCreateInfo* pCreateInfo, Subprogram** ppInstance);

        void Destroy()
        {
            const Device* pDevice = &m_device;
            this->~Subprogram();
            rps::Free(pDevice->Allocator(), this);
        }

        Arena& GetArena()
        {
            return m_arena;
        }

        const RpslEntry* GetEntry() const
        {
            return m_pEntry;
        }

        const RenderGraphSignature* GetSignature() const
        {
            return m_pSignature;
        }

        const RpslNodeImpl& GetNodeImpl(uint32_t localNodeDeclId) const
        {
            return m_nodeImpls[localNodeDeclId];
        }

        const RpsCmdCallback& GetDefaultNodeCallback() const
        {
            return m_defaultNodeImpl.callback;
        }

        RpsResult BindDefaultCallback(const RpsCmdCallback& callback)
        {
            m_defaultNodeImpl.callback = callback;
            return RPS_OK;
        }

        RpsResult Bind(const StrRef name, Subprogram* pRpslEntry)
        {
            const uint32_t nodeDeclId = m_pSignature->FindNodeDeclIndexByName(name);
            RPS_RETURN_ERROR_IF(nodeDeclId == RPS_INDEX_NONE_U32, RPS_ERROR_UNKNOWN_NODE);

            return Bind(nodeDeclId, pRpslEntry);
        }

        RpsResult Bind(StrRef name, const RpsCmdCallback& callback)
        {
            if (name.empty())
            {
                return BindDefaultCallback(callback);
            }
            else
            {
                const uint32_t nodeDeclId = m_pSignature->FindNodeDeclIndexByName(name);
                RPS_RETURN_ERROR_IF(nodeDeclId == RPS_INDEX_NONE_U32, RPS_ERROR_UNKNOWN_NODE);

                return Bind(nodeDeclId, callback);
            }
        }

        RpsResult Bind(uint32_t nodeDeclId, Subprogram* pRpslEntry)
        {
            RPS_CHECK_ARGS(nodeDeclId < m_nodeImpls.size());

            m_nodeImpls[nodeDeclId].Set(pRpslEntry);

            return RPS_OK;
        }

        RpsResult Bind(uint32_t nodeDeclId, const RpsCmdCallback& callback)
        {
            RPS_CHECK_ARGS(nodeDeclId < m_nodeImpls.size());

            m_nodeImpls[nodeDeclId].Set(callback);

            return RPS_OK;
        }

        RpsResult BindDeferred(StrRef name, size_t contextSize, RpsCmdCallback** ppCallback)
        {
            if (name.empty())
            {
                RPS_V_RETURN(InitNodeImplForContext(m_defaultNodeImpl, contextSize));
                *ppCallback = &m_defaultNodeImpl.callback;
                return RPS_OK;
            }

            uint32_t nodeDeclId = m_pSignature->FindNodeDeclIndexByName(name);
            RPS_RETURN_ERROR_IF(nodeDeclId == RPS_INDEX_NONE_U32, RPS_ERROR_UNKNOWN_NODE);

            return BindDeferred(nodeDeclId, contextSize, ppCallback);
        }

        RpsResult BindDeferred(uint32_t nodeDeclId, size_t contextSize, RpsCmdCallback** ppCallback)
        {
            RPS_CHECK_ARGS(ppCallback != nullptr);
            RPS_CHECK_ARGS(nodeDeclId < m_nodeImpls.size());

            auto& nodeImpl = m_nodeImpls[nodeDeclId];
            RPS_V_RETURN(InitNodeImplForContext(nodeImpl, contextSize));

            *ppCallback = &nodeImpl.callback;

            return RPS_OK;
        }

    private:
        RpsResult InitNodeImplForContext(Subprogram::RpslNodeImpl& nodeImpl, size_t contextSize)
        {
            if ((nodeImpl.pBuffer == nullptr) || (nodeImpl.bufferSize < contextSize))
            {
                nodeImpl.bufferSize = uint32_t(contextSize);
                nodeImpl.pBuffer    = m_arena.Alloc(contextSize);
                RPS_CHECK_ALLOC(nodeImpl.pBuffer);
            }

            nodeImpl.Set(RpsCmdCallback{PFN_rpsCmdCallback(nullptr), nodeImpl.pBuffer, RPS_CMD_CALLBACK_FLAG_NONE});

            return RPS_OK;
        }

    private:
        RpsResult Init(const RpsProgramCreateInfo* pCreateInfo);

    private:
        const Device&               m_device;
        Arena                       m_arena;
        const RenderGraphSignature* m_pSignature = nullptr;
        const RpslEntry* const      m_pEntry;

        ArrayRef<RpslNodeImpl> m_nodeImpls;
        RpslNodeImpl           m_defaultNodeImpl;
    };

    RPS_ASSOCIATE_HANDLE(Subprogram);

}  // namespace rps

#endif  //RPS_SUBPROGRAM_HPP
