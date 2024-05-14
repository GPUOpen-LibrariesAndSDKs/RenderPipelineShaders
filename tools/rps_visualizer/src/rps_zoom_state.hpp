// Copyright (c) 2024 Advanced Micro Devices, Inc.
//
// This file is part of the AMD Render Pipeline Shaders SDK which is
// released under the MIT LICENSE.
//
// See file LICENSE.txt for full license details.

#ifndef RPS_ZOOM_STATE_HPP
#define RPS_ZOOM_STATE_HPP

#include "rps_visualizer_util.hpp"

namespace rps
{
    class ZoomState
    {
    public:
        RpsResult SetUpperBound(uint64_t units)
        {
            // No support for units > INT64_MAX as we cast unit differences to int64_t for simplicity
            RPS_RETURN_ERROR_IF(units > INT64_MAX, RPS_ERROR_INTEGER_OVERFLOW);

            const bool bTotalUnitsChanged = units != m_totalUnits;

            m_totalUnits        = units;
            m_visibleRangeBegin = bTotalUnitsChanged ? 0 : m_visibleRangeBegin;
            m_visibleRangeEnd   = bTotalUnitsChanged ? units : m_visibleRangeEnd;

            return RPS_OK;
        }

        void SetDisplayedPixels(float numPixels)
        {
            m_displayedPixels = rpsMax(1.0f, numPixels);
        }

        uint64_t GetTickInterval(uint64_t roundToMultiplesOf = 10) const
        {
            const int64_t minTickUnits = rpsMax(PixelsToUnits(MinTickPixelInterval), int64_t(1));
            return (minTickUnits == 1) ? minTickUnits
                                       : rpsMin(m_totalUnits, RoundUpToMultiplesOf(minTickUnits, roundToMultiplesOf));
        }

        uint64_t GetVisibleRangeUnits() const
        {
            return m_visibleRangeEnd - m_visibleRangeBegin;
        }

        uint64_t GetVisibleRangeBegin() const
        {
            return m_visibleRangeBegin;
        }

        uint64_t GetVisibleRangeEnd() const
        {
            return m_visibleRangeEnd;
        }

        uint64_t GetTotalUnits() const
        {
            return m_totalUnits;
        }

        float GetTotalRangeInPixels() const
        {
            return float(UnitsToPixels(m_totalUnits));
        }

        float GetScrollRatio() const
        {
            return float(double(m_visibleRangeBegin) / m_totalUnits);
        }

        float GetScrollInPixels() const
        {
            return GetScrollRatio() * GetTotalRangeInPixels();
        }

        void SetScrollInPixels(float scrollPos)
        {
            MoveByPixels(scrollPos - GetScrollInPixels());
        }

        float GetZoomLevel() const
        {
            return float(double(m_totalUnits) / GetVisibleRangeUnits());
        }

        uint64_t Pick(float pixelPos, float* pFract = nullptr) const
        {
            const double t = rpsClamp(double(pixelPos) / m_displayedPixels, 0.0, 1.0) * GetVisibleRangeUnits();
            if (pFract)
            {
                *pFract = float(t - floor(t));
            }
            return uint64_t(floor(t)) + m_visibleRangeBegin;
        }

        void ZoomToPixelRange(ImVec2 pixelRange)
        {
            const uint64_t rangeBegin = Pick(pixelRange.x);
            const uint64_t rangeEnd   = Pick(pixelRange.y);
            ZoomToUnitRange(rangeBegin, rangeEnd);
        }

        void ZoomToUnitRange(uint64_t rangeBegin, uint64_t rangeEnd)
        {
            m_visibleRangeBegin = rangeBegin;
            m_visibleRangeEnd   = rpsMax(rangeEnd, m_visibleRangeBegin + 1);
        }

        void ZoomByMultiplier(float zoomLevelMultiplier, float pivot = 0.0f)
        {
            const uint64_t pivotUnit        = Pick(pivot);
            const double   actualPivotRatio = (double(pivotUnit) - m_visibleRangeBegin) / GetVisibleRangeUnits();

            const double desiredVisibleUnits = GetVisibleRangeUnits() / rpsClamp(zoomLevelMultiplier, 0.5f, 2.0f);
            const double pivotUnitOffset     = desiredVisibleUnits * actualPivotRatio;

            const uint64_t newBegin = uint64_t(rpsMax(int64_t(0), int64_t(round(pivotUnit - pivotUnitOffset))));
            const uint64_t newEnd   = rpsMin(newBegin + uint64_t(round(desiredVisibleUnits)), m_totalUnits);

            // Prevent lock-in when one step of zoom is not big enough to jump to the next level due to clamping
            if ((newBegin == m_visibleRangeBegin) && (newEnd == m_visibleRangeEnd) && (zoomLevelMultiplier < 1.0f) &&
                (zoomLevelMultiplier > 0.5f))
            {
                ZoomByMultiplier(0.5f, pivot);
                return;
            }

            m_visibleRangeBegin = newBegin;
            m_visibleRangeEnd   = newEnd;
        }

        void ZoomToLevel(float zoomLevel, float pivot = 0.0f)
        {
        }

        void MoveByUnits(int64_t deltaUnits)
        {
            //Avoid decreasing size of the range when moving against a border by decreasing the step size in those cases.
            const int64_t clampedDeltaUnits = deltaUnits < 0
                                                  ? -rpsMin(-deltaUnits, int64_t(m_visibleRangeBegin))
                                                  : rpsMin(deltaUnits, int64_t(m_totalUnits - m_visibleRangeEnd));

            m_visibleRangeBegin = m_visibleRangeBegin + clampedDeltaUnits;
            m_visibleRangeEnd   = m_visibleRangeEnd + clampedDeltaUnits;
        }

        void MoveByPixels(float pixels)
        {
            const int64_t units = PixelsToUnits(pixels);
            MoveByUnits(units);
        }

        int64_t PixelsToUnits(float pixels) const
        {
            return int64_t(round(double(pixels) / m_displayedPixels * GetVisibleRangeUnits()));
        }

        float UnitsToPixels(int64_t units) const
        {
            return float(double(m_displayedPixels) / GetVisibleRangeUnits() * units);
        }

        float UnitToPixelOffset(uint64_t unit) const
        {
            return UnitsToPixels(int64_t(unit) - int64_t(GetVisibleRangeBegin()));
        }

    private:
        static constexpr float MinTickPixelInterval = 10.f;

        float    m_displayedPixels   = 1.0f;
        uint64_t m_totalUnits        = 1;
        uint64_t m_visibleRangeBegin = 0;
        uint64_t m_visibleRangeEnd   = 1;
    };

}  // namespace rps

#endif //RPS_ZOOM_STATE_HPP
