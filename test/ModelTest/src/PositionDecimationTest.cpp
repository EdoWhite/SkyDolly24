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
#include <vector>

#include <QtTest>

#include <Model/PositionData.h>
#include <Model/PositionDecimation.h>

#include "PositionDecimationTest.h"

namespace
{
    // One frame at 60 Hz
    constexpr std::int64_t FrameMSec {17};

    PositionData at(std::int64_t timestamp, double latitude, double longitude, double altitude)
    {
        PositionData position;
        position.timestamp = timestamp;
        position.latitude = latitude;
        position.longitude = longitude;
        position.altitude = altitude;
        return position;
    }

    // A flight recorded at frame rate. The generator is given the position at a time, so that the
    // same track can be produced densely (what the simulator sends) and then compared against what
    // survives decimation.
    template <typename Generator>
    std::vector<PositionData> record(Generator generator, int frames)
    {
        std::vector<PositionData> samples;
        samples.reserve(static_cast<std::size_t>(frames));
        for (int i = 0; i < frames; ++i) {
            samples.push_back(generator(i * ::FrameMSec));
        }
        return samples;
    }

    // Runs the samples through the decimation exactly as the connect plugin does
    std::vector<PositionData> decimate(const std::vector<PositionData> &samples)
    {
        std::vector<PositionData> kept;
        for (const PositionData &sample : samples) {
            if (kept.size() < 2 ||
                PositionDecimation::shouldStore(kept[kept.size() - 2], kept.back(), sample)) {
                kept.push_back(sample);
            }
        }
        return kept;
    }

    // The worst distance between the original track and the decimated one, measured by linearly
    // interpolating the kept samples at every original timestamp: what a replay would show
    double worstDeviation(const std::vector<PositionData> &original, const std::vector<PositionData> &kept)
    {
        double worst {0.0};
        std::size_t segment {0};
        for (const PositionData &sample : original) {
            while (segment + 2 < kept.size() && kept[segment + 1].timestamp < sample.timestamp) {
                ++segment;
            }
            const PositionData &a = kept[segment];
            const PositionData &b = kept[segment + 1 < kept.size() ? segment + 1 : segment];
            const auto span = b.timestamp - a.timestamp;
            const double mu = span > 0 ?
                static_cast<double>(sample.timestamp - a.timestamp) / static_cast<double>(span) : 0.0;
            PositionData interpolated;
            interpolated.latitude = a.latitude + (b.latitude - a.latitude) * mu;
            interpolated.longitude = a.longitude + (b.longitude - a.longitude) * mu;
            interpolated.altitude = a.altitude + (b.altitude - a.altitude) * mu;
            worst = std::max(worst, PositionDecimation::distanceTo(interpolated, sample));
        }
        return worst;
    }
}

// PRIVATE SLOTS

void PositionDecimationTest::initTestCase() noexcept
{}

void PositionDecimationTest::cleanupTestCase() noexcept
{}

void PositionDecimationTest::keepsEverythingUntilThereIsADirection() noexcept
{
    // Setup: two samples with no time between them carry no direction to predict along
    const PositionData a = ::at(0, 47.0, 8.0, 1000.0);
    const PositionData b = ::at(0, 47.0, 8.0, 1000.0);
    const PositionData c = ::at(17, 47.0001, 8.0, 1000.0);

    // Exercise / Verify: the opening samples of a recording are what a replay starts from, so
    // they are never guessed at
    QVERIFY(PositionDecimation::shouldStore(a, b, c));
}

void PositionDecimationTest::dropsSamplesOnAStraightLeg() noexcept
{
    // Setup: due north at a constant rate, which is perfectly described by its endpoints
    const auto straight = [](std::int64_t t) {
        return ::at(t, 47.0 + 0.00001 * static_cast<double>(t), 8.0, 1000.0);
    };
    const std::vector<PositionData> samples = ::record(straight, 120);

    // Exercise
    const std::vector<PositionData> kept = ::decimate(samples);

    // Verify: only the samples forced by the maximum interval survive
    QVERIFY2(kept.size() < samples.size() / 10,
             qPrintable(QString("kept %1 of %2").arg(kept.size()).arg(samples.size())));
}

void PositionDecimationTest::keepsSamplesThroughATurn() noexcept
{
    // Setup: a turn of about 1.5 degrees per second, which is what the old 1 Hz sampling could
    // not describe
    const auto turn = [](std::int64_t t) {
        const double seconds = static_cast<double>(t) / 1000.0;
        const double angle = seconds * 0.4;
        return ::at(t, 47.0 + 0.005 * std::sin(angle), 8.0 + 0.005 * (1.0 - std::cos(angle)), 1000.0);
    };
    const std::vector<PositionData> samples = ::record(turn, 300);

    // Exercise
    const std::vector<PositionData> kept = ::decimate(samples);

    // Verify: a bending track has to keep far more than a straight one
    QVERIFY2(kept.size() > samples.size() / 20,
             qPrintable(QString("kept only %1 of %2 through a turn").arg(kept.size()).arg(samples.size())));
}

void PositionDecimationTest::alwaysStoresAfterTheMaximumInterval() noexcept
{
    // Setup: a parked aircraft, where the prediction is perfect forever
    const PositionData a = ::at(0, 47.0, 8.0, 1000.0);
    const PositionData b = ::at(17, 47.0, 8.0, 1000.0);

    // Exercise / Verify
    QVERIFY(!PositionDecimation::shouldStore(a, b, ::at(500, 47.0, 8.0, 1000.0)));
    QVERIFY(PositionDecimation::shouldStore(a, b, ::at(1017, 47.0, 8.0, 1000.0)));
}

void PositionDecimationTest::keepsAClimbThatStartsMidLeg() noexcept
{
    // Setup: level flight, then the aircraft starts climbing. Altitude alone must be enough to
    // trigger a sample: an aeroplane that rotates is not doing nothing.
    const PositionData a = ::at(0, 47.0, 8.0, 1000.0);
    const PositionData b = ::at(100, 47.0, 8.0, 1000.0);

    // Exercise / Verify: three metres of climb is well past the half metre threshold
    QVERIFY(PositionDecimation::shouldStore(a, b, ::at(200, 47.0, 8.0, 1010.0)));
    // ... while a centimetre of settling is not
    QVERIFY(!PositionDecimation::shouldStore(a, b, ::at(200, 47.0, 8.0, 1000.03)));
}

void PositionDecimationTest::decimatedTrackStaysWithinThreshold() noexcept
{
    // Setup: a climbing turn, the case where all three coordinates change at once
    const auto climbingTurn = [](std::int64_t t) {
        const double seconds = static_cast<double>(t) / 1000.0;
        const double angle = seconds * 0.3;
        return ::at(t,
                    47.0 + 0.01 * std::sin(angle),
                    8.0 + 0.01 * (1.0 - std::cos(angle)),
                    1000.0 + seconds * 12.0);
    };
    const std::vector<PositionData> samples = ::record(climbingTurn, 600);

    // Exercise
    const std::vector<PositionData> kept = ::decimate(samples);
    const double worst = ::worstDeviation(samples, kept);

    // Verify: this is the whole justification for throwing samples away. The bound is generous
    // against the half metre threshold because the threshold governs one step ahead, while error
    // accumulates across a kept segment.
    QVERIFY2(worst < 5.0, qPrintable(QString("worst deviation %1 m over %2 kept of %3 samples")
                                     .arg(worst).arg(kept.size()).arg(samples.size())));
}

void PositionDecimationTest::straightAndLevelFlightCostsAlmostNothing() noexcept
{
    // Setup: ten minutes of cruise at frame rate, which is what a long flight mostly consists of
    const auto cruise = [](std::int64_t t) {
        return ::at(t, 47.0, 8.0 + 0.000004 * static_cast<double>(t), 35000.0);
    };
    const std::vector<PositionData> samples = ::record(cruise, 60 * 600);

    // Exercise
    const std::vector<PositionData> kept = ::decimate(samples);

    // Verify: without decimation this leg would cost 36000 rows. It should come out close to one
    // per second, which is what the logbook held before position moved to frame rate.
    QVERIFY2(kept.size() < 1200, qPrintable(QString("kept %1 rows for ten minutes of cruise")
                                            .arg(kept.size())));
    QVERIFY(::worstDeviation(samples, kept) < 5.0);
}

QTEST_MAIN(PositionDecimationTest)
