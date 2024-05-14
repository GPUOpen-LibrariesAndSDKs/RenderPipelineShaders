// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_SELECTOR_STATE_HPP
#define RPS_SELECTOR_STATE_HPP

#include "rps_visualizer_util.hpp"

namespace rps
{
    class SelectorState
    {
    public:
        void BeginDrag(uint64_t beginPos)
        {
            m_bIsDragging    = true;
            m_selectionRange = {beginPos, beginPos};
        }

        void EndDrag()
        {
            m_bIsDragging = false;
        }

        void DragTo(uint64_t currPos)
        {
            m_selectionRange.y = currPos;
        }

        bool IsDragging() const
        {
            return m_bIsDragging;
        }

        void ClearSelection()
        {
            m_selectionRange.y = m_selectionRange.x;
        }

        bool HasSelection() const
        {
            return m_selectionRange.x != m_selectionRange.y;
        }

        void SetSelectionRange(const U64Vec2& range)
        {
            BeginDrag(range.x);
            DragTo(range.y);
            EndDrag();
        }

        const U64Vec2& GetSelectionRange() const
        {
            return m_selectionRange;
        }

        U64Vec2 GetSelectionRangeOrdered() const
        {
            return (m_selectionRange.y >= m_selectionRange.x) ? m_selectionRange
                                                              : U64Vec2{m_selectionRange.y, m_selectionRange.x};
        }

    private:
        U64Vec2 m_selectionRange = {};
        bool    m_bIsDragging    = false;
    };

}  // namespace rps

#endif  //RPS_SELECTOR_STATE_HPP
