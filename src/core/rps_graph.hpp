// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_GRAPH_HPP
#define RPS_GRAPH_HPP

#include "rps_core.hpp"
#include "rps_util.hpp"
#include "rps_device.hpp"

namespace rps
{
    using NodeId = uint32_t;
    using SubgraphId = uint32_t;

    struct Edge
    {
        NodeId src;
        NodeId dst;
    };

    struct Node
    {
        Span<Edge> inEdges;
        Span<Edge> outEdges;
        int32_t    cmdId;
        uint32_t   subgraph     = RPS_INDEX_NONE_U32;
        uint32_t   barrierScope = 0;

        Node()
        {
        }

        Node(int32_t inCmdId)
            : cmdId(inCmdId)
        {
        }

        bool IsTransition() const
        {
            return cmdId < 0;
        }

        uint32_t GetTransitionId() const
        {
            RPS_ASSERT(IsTransition());
            return uint32_t(-cmdId);
        }

        uint32_t GetCmdId() const
        {
            RPS_ASSERT(!IsTransition());
            return uint32_t(cmdId);
        }
    };

    struct Subgraph
    {
        uint32_t         parentSubgraph;
        RpsSubgraphFlags flags;
        NodeId           beginNode;
        NodeId           endNode;

        Subgraph() = default;

        Subgraph(uint32_t inParentSubgraph, RpsSubgraphFlags inFlags, NodeId inBeginNode)
            : parentSubgraph(inParentSubgraph)
            , flags(inFlags)
            , beginNode(inBeginNode)
            , endNode(inBeginNode)
        {
        }

        bool IsAtomic() const
        {
            return rpsAnyBitsSet(flags, RPS_SUBGRAPH_FLAG_ATOMIC);
        }

        bool IsSequential() const
        {
            return rpsAnyBitsSet(flags, RPS_SUBGRAPH_FLAG_SEQUENTIAL);
        }
    };

    class Graph
    {
    public:
        Graph(const Device& device, Arena& arena);

        Node* GetNode(NodeId nodeId)
        {
            return &m_nodes[nodeId];
        }
        const Node* GetNode(NodeId nodeId) const
        {
            return &m_nodes[nodeId];
        }

        ConstArrayRef<Node> GetNodes() const
        {
            return m_nodes.range_all();
        }

        ConstArrayRef<Edge> GetEdges() const
        {
            return m_edges.range_all();
        }

        ConstArrayRef<Subgraph> GetSubgraphs() const
        {
            return m_subgraphs.range_all();
        }

        NodeId AddNode(int32_t cmdId);
        NodeId CloneNode(NodeId srcNode, int32_t cmdId);

        void AddEdge(NodeId fromNode, NodeId toNode)
        {
            AddToEdgeList(m_nodes[fromNode].outEdges, Edge{fromNode, toNode});
            AddToEdgeList(m_nodes[toNode].inEdges, Edge{fromNode, toNode});
        }

        SubgraphId AddSubgraph(uint32_t parentId, RpsSubgraphFlags flags, NodeId beginNode)
        {
            SubgraphId resultId = SubgraphId(m_subgraphs.size());
            m_subgraphs.push_back(Subgraph{parentId, flags, beginNode});
            return ((resultId + 1) == m_subgraphs.size()) ? resultId : RPS_INDEX_NONE_U32;
        }

        Subgraph* GetSubgraph(SubgraphId subgraphId)
        {
            return &m_subgraphs[subgraphId];
        }

        bool IsParentSubgraph(uint32_t parentSubgraphId, uint32_t childSubgraphId) const;

        void Reset();

    private:
        void AddToEdgeList(Span<Edge>& edgeList, Edge newEdge);

    private:
        ArenaVector<Node>     m_nodes;
        ArenaVector<Edge>     m_edges;
        ArenaVector<Subgraph> m_subgraphs;

        SpanPool<Edge, ArenaVector<Edge>> m_edgeListPool;
    };
}

#endif  //RPS_GRAPH_HPP
