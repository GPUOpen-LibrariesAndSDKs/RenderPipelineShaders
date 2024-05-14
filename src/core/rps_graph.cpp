// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#include "core/rps_graph.hpp"
#include "core/rps_device.hpp"
#include "core/rps_util.hpp"

namespace rps
{
    Graph::Graph(const Device& device, Arena& arena)
        : m_nodes(&arena)
        , m_edges(&arena)
        , m_subgraphs(&arena)
        , m_edgeListPool(m_edges)
    {
    }

    NodeId Graph::AddNode(int32_t cmdId)
    {
        NodeId nodeId = NodeId(m_nodes.size());
        m_nodes.emplace_back(cmdId);
        return nodeId;
    }

    NodeId Graph::CloneNode(NodeId srcNodeId, int32_t cmdId)
    {
        NodeId newNodeId = AddNode(cmdId);

        RPS_RETURN_ERROR_IF(newNodeId == RPS_INDEX_NONE_U32, newNodeId);

        const Node* pSrcNode = GetNode(srcNodeId);
        Node*       pNewNode = GetNode(newNodeId);

        pNewNode->barrierScope = pSrcNode->barrierScope;
        pNewNode->subgraph     = pSrcNode->subgraph;

        const size_t edgeCapacity = m_edges.size() + (pSrcNode->inEdges.size() + pSrcNode->outEdges.size()) * 2;
        m_edges.reserve(edgeCapacity);

        RPS_RETURN_ERROR_IF(m_edges.capacity() < edgeCapacity, RPS_INDEX_NONE_U32);

        for (auto e : pSrcNode->inEdges.Get(m_edges))
        {
            AddEdge(e.src, newNodeId);
        }

        for (auto e : pSrcNode->outEdges.Get(m_edges))
        {
            AddEdge(newNodeId, e.dst);
        }

        return newNodeId;
    }

    void Graph::AddToEdgeList(Span<Edge>& edgeList, Edge newEdge)
    {
        m_edgeListPool.push_to_span(edgeList, newEdge);
    }

    bool Graph::IsParentSubgraph(uint32_t parentSubgraphId, uint32_t childSubgraphId) const
    {
        if ((parentSubgraphId == RPS_INDEX_NONE_U32) || (childSubgraphId == RPS_INDEX_NONE_U32))
        {
            return false;
        }

        uint32_t currIdx = childSubgraphId;

        while ((parentSubgraphId < currIdx) && (currIdx != RPS_INDEX_NONE_U32))
        {
            currIdx = m_subgraphs[currIdx].parentSubgraph;
        }

        return currIdx == parentSubgraphId;
    }

    void Graph::Reset()
    {
        m_nodes.reset();
        m_edges.reset();
        m_subgraphs.reset();
        m_edgeListPool.reset();
    }

}  // namespace rps
