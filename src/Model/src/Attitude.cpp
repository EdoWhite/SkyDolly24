/**
 * Sky Dolly - The black sheep for your fposition recordings
 *
 * Copyright (c) 2020 - 2025 Oliver Knoll
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons
 * to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED *AS IS*, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE
 * FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#include <algorithm>
#include <cstdint>

#ifdef DEBUG
#include <QDebug>
#endif

#include <Kernel/SkyMath.h>
#include "TimeVariableData.h"
#include "SkySearch.h"
#include "AircraftInfo.h"
#include "AttitudeData.h"
#include "Attitude.h"

// PUBLIC

Attitude::Attitude(const AircraftInfo &aircraftInfo) noexcept
    : AbstractComponent(aircraftInfo)
{}

const AttitudeData &Attitude::interpolate(std::int64_t timestamp, TimeVariableData::Access access) const noexcept
{
    const AttitudeData *p0 {nullptr}, *p1 {nullptr}, *p2 {nullptr}, *p3 {nullptr};
    const auto timeOffset = access != TimeVariableData::Access::NoTimeOffset ? getAircraftInfo().timeOffset : 0;
    const auto adjustedTimestamp = std::max(timestamp + timeOffset, std::int64_t(0));

    if (getCurrentTimestamp() != adjustedTimestamp || getCurrentAccess() != access) {
        int currentIndex = getCurrentIndex();
        double tn {0.0};
        // Attitude data is always interpolated within an "infinite" interpolation window, in order to
        // take imported "sparse flight plans" into account
        if (SkySearch::getCubicInterpolationSupportData(getData(), adjustedTimestamp, SkySearch::InfinitetInterpolationWindow, currentIndex, &p0, &p1, &p2, &p3)) {
            tn = SkySearch::normaliseTimestamp(*p1, *p2, adjustedTimestamp);
        }
        if (p1 != nullptr) {
            // Aircraft attitude.
            //
            // Pitch, bank and heading are one rotation, not three independent numbers. They used to
            // be interpolated as three separate cubic splines, which lets the intermediate
            // attitudes leave the path actually flown - worst exactly where all three change at
            // once, which is a turn, and visible as the aircraft rocking about its own axis while
            // the recording did no such thing. Interpolating on the unit sphere removes that by
            // construction, and takes the wrap at 0/360 and +/-180 with it, since a quaternion has
            // no discontinuity to wrap around.
            const SkyMath::Quaternion q0 = SkyMath::quaternionFromEuler(p0->pitch, p0->bank, p0->trueHeading);
            const SkyMath::Quaternion q1 = SkyMath::quaternionFromEuler(p1->pitch, p1->bank, p1->trueHeading);
            const SkyMath::Quaternion q2 = SkyMath::quaternionFromEuler(p2->pitch, p2->bank, p2->trueHeading);
            const SkyMath::Quaternion q3 = SkyMath::quaternionFromEuler(p3->pitch, p3->bank, p3->trueHeading);
            const SkyMath::Quaternion interpolated = SkyMath::squad(q0, q1, q2, q3, tn);
            SkyMath::eulerFromQuaternion(interpolated, m_currentData.pitch, m_currentData.bank,
                                         m_currentData.trueHeading);

            // Velocity
            m_currentData.velocityBodyX = SkyMath::interpolateLinear(p1->velocityBodyX, p2->velocityBodyX, tn);
            m_currentData.velocityBodyY = SkyMath::interpolateLinear(p1->velocityBodyY, p2->velocityBodyY, tn);
            m_currentData.velocityBodyZ = SkyMath::interpolateLinear(p1->velocityBodyZ, p2->velocityBodyZ, tn);

            // On ground (boolean value - no interpolation)
            m_currentData.onGround = p1->onGround;

            m_currentData.timestamp = adjustedTimestamp;
        } else {
            // No recorded data, or the timestamp exceeds the timestamp of the last recorded data
            m_currentData.reset();
        }

        setCurrentIndex(currentIndex);
        setCurrentTimestamp(adjustedTimestamp);
        setCurrentAccess(access);
    }
    return m_currentData;
}

template class AbstractComponent<AttitudeData>;
