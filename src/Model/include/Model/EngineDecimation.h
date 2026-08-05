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
#ifndef ENGINEDECIMATION_H
#define ENGINEDECIMATION_H

#include <cstdint>

#include "EngineData.h"
#include "ModelLib.h"

/*!
 * Decides which engine samples are worth keeping.
 *
 * The engine record is requested with SIMCONNECT_DATA_REQUEST_FLAG_CHANGED, so until now it only
 * arrived when something in it moved - and lever positions and switches sit perfectly still through
 * an entire cruise. Engine speeds do not: they wander by a few revolutions per minute constantly,
 * so from the moment they joined the record it changes on every single frame. Left alone that would
 * turn a sparse table into a sixty-a-second one, and almost every row of it would say nothing.
 *
 * So a sample is stored only when it carries information. Anything discrete - a starter, a
 * magneto's worth of combustion, a battery - is stored the instant it changes, because those are
 * the moments a replay has to get exactly right. A lever that moves is stored for the same reason.
 * The engine speeds, being smooth, are allowed to drift up to a threshold before they cost a row.
 *
 * The result is bounded: a replayed engine speed is never further from the recorded one than that
 * threshold, which is what makes this safe to do at all.
 *
 * \sa PositionDecimation
 */
class MODEL_API EngineDecimation final
{
public:
    /*!
     * How far an engine may drift from the last stored sample before a new one has to be stored
     * [revolutions per minute]. Five revolutions is two thousandths of a piston engine at cruise:
     * neither audible nor visible on a gauge.
     */
    static constexpr float DefaultRpmThreshold {5.0f};

    /*!
     * The same, for the turbines' low pressure spool [percent]. A quarter of a percent is finer
     * than an N1 gauge is read to.
     */
    static constexpr float DefaultN1Threshold {0.25f};

    /*!
     * The longest a sample may be skipped, whatever else is true [milliseconds]. Bounds the gap the
     * interpolation has to bridge on a seek, and keeps an idling aircraft from producing an engine
     * track with no points in it at all.
     */
    static constexpr std::int64_t DefaultMaximumInterval {1000};

    /*!
     * Returns whether \p candidate has to be stored, given that \p lastStored was the last sample
     * actually written.
     *
     * \param lastStored
     *        the most recently stored engine sample
     * \param candidate
     *        the newly received engine sample
     * \param rpmThreshold
     *        the engine speed drift that forces a store [revolutions per minute]
     * \param n1Threshold
     *        the turbine spool drift that forces a store [percent]
     * \param maximumInterval
     *        the longest a sample may be skipped [milliseconds]
     * \return \c true if \p candidate has to be stored; \c false if it may be dropped
     */
    static bool shouldStore(const EngineData &lastStored, const EngineData &candidate,
                            float rpmThreshold = DefaultRpmThreshold,
                            float n1Threshold = DefaultN1Threshold,
                            std::int64_t maximumInterval = DefaultMaximumInterval) noexcept;
};

#endif // ENGINEDECIMATION_H
