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
#ifndef POSITIONDECIMATION_H
#define POSITIONDECIMATION_H

#include <cstdint>

#include "PositionData.h"
#include "ModelLib.h"

/*!
 * Decides which position samples are worth keeping.
 *
 * Position is now sampled once per simulator frame rather than once per second, because a one
 * second gap is far too long to guess a turn across: that gap, not the interpolation method, is
 * what makes a replayed turn cut corners and drift out of step with the attitude. Sampling sixty
 * times faster would also make the logbook sixty times larger, and almost all of it would be
 * redundant - straight and level flight is perfectly described by its endpoints.
 *
 * So a sample is stored only when it carries information: when the aircraft has actually gone
 * somewhere other than where the previous two samples said it was heading. On a straight leg that
 * keeps almost nothing; in a turn it keeps almost everything, which is exactly the right way round.
 *
 * The result is bounded: no stored track ever departs from the flown one by more than the
 * threshold, which is what makes this safe to do at all.
 */
class MODEL_API PositionDecimation final
{
public:
    /*!
     * How far the aircraft may be from where the previous samples predicted before a new sample
     * has to be stored [meters]. Half a metre is well below what is visible from the cockpit at
     * any speed, and still throws away the great majority of straight and level flight.
     */
    static constexpr double DefaultPositionThreshold {0.5};

    /*!
     * The longest a sample may be skipped, whatever the prediction says [milliseconds]. Bounds the
     * gap the interpolation has to bridge on seek, and keeps a paused or parked aircraft from
     * producing a track with no points in it at all.
     */
    static constexpr std::int64_t DefaultMaximumInterval {1000};

    /*!
     * Returns whether \p candidate has to be stored.
     *
     * \param beforePrevious
     *        the sample stored before \p previous; predictions need two points to have a direction
     * \param previous
     *        the most recently stored sample
     * \param candidate
     *        the sample just received
     * \param positionThreshold
     *        the deviation beyond which the candidate is kept [meters]
     * \param maximumInterval
     *        the longest gap tolerated regardless of deviation [milliseconds]
     * \return \c true if \p candidate must be stored; \c false if it is close enough to what
     *         interpolation would produce anyway
     */
    static bool shouldStore(const PositionData &beforePrevious, const PositionData &previous,
                            const PositionData &candidate,
                            double positionThreshold = DefaultPositionThreshold,
                            std::int64_t maximumInterval = DefaultMaximumInterval) noexcept;

    /*!
     * Returns the distance between two positions [meters], including the altitude difference.
     *
     * A local flat-earth approximation: samples are milliseconds and metres apart, where the
     * error of that approximation is far below the threshold it is compared against.
     */
    static double distanceTo(const PositionData &from, const PositionData &to) noexcept;
};

#endif // POSITIONDECIMATION_H
