/**
 * Sky Dolly - The Black Sheep for Your Flight Recordings
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
#include <cmath>
#include <cstdint>

#include <Kernel/SkyMath.h>
#include <Kernel/Convert.h>

#include "PositionData.h"
#include "PositionDecimation.h"

namespace
{
    // Feet to meters, for the altitude difference
    constexpr double FeetToMeters {0.3048};
}

// PUBLIC

double PositionDecimation::distanceTo(const PositionData &from, const PositionData &to) noexcept
{
    const double meanLatitude = Convert::degreesToRadians((from.latitude + to.latitude) / 2.0);
    const double deltaLatitude = Convert::degreesToRadians(to.latitude - from.latitude) * SkyMath::EarthRadius;
    const double deltaLongitude = Convert::degreesToRadians(to.longitude - from.longitude) *
                                  SkyMath::EarthRadius * std::cos(meanLatitude);
    const double deltaAltitude = (to.altitude - from.altitude) * ::FeetToMeters;

    return std::sqrt(deltaLatitude * deltaLatitude +
                     deltaLongitude * deltaLongitude +
                     deltaAltitude * deltaAltitude);
}

bool PositionDecimation::shouldStore(const PositionData &beforePrevious, const PositionData &previous,
                                     const PositionData &candidate, double positionThreshold,
                                     std::int64_t maximumInterval) noexcept
{
    // Never drop a sample that closes a gap longer than the interpolation should have to bridge
    if (candidate.timestamp - previous.timestamp >= maximumInterval) {
        return true;
    }

    // Without two earlier samples there is no direction to predict along, so keep everything until
    // there is: the opening samples of a recording are the ones a replay starts from
    const std::int64_t previousInterval = previous.timestamp - beforePrevious.timestamp;
    if (previousInterval <= 0) {
        return true;
    }

    // Where the aircraft would be if it had carried on as the last two samples suggest. Linear
    // rather than cubic on purpose: the question is whether the track is bending, and a predictor
    // that can itself bend would answer "no" through a turn.
    const double scale = static_cast<double>(candidate.timestamp - previous.timestamp) /
                         static_cast<double>(previousInterval);
    PositionData predicted;
    predicted.latitude = previous.latitude + (previous.latitude - beforePrevious.latitude) * scale;
    predicted.longitude = previous.longitude + (previous.longitude - beforePrevious.longitude) * scale;
    predicted.altitude = previous.altitude + (previous.altitude - beforePrevious.altitude) * scale;

    return distanceTo(predicted, candidate) > positionThreshold;
}
