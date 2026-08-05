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
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include <QtTest>

#include <Model/EngineData.h>
#include <Model/EngineDecimation.h>

#include "EngineDecimationTest.h"

namespace
{
    // One frame at 60 Hz
    constexpr std::int64_t FrameMSec {17};

    EngineData idling(std::int64_t timestamp)
    {
        EngineData engine;
        engine.timestamp = timestamp;
        engine.throttleLeverPosition1 = 1000;
        engine.mixtureLeverPosition1 = 100;
        engine.generalEngineCombustion1 = true;
        engine.generalEngineRpm1 = 800.0f;
        engine.turbineEngineN1Percent1 = 22.0f;
        return engine;
    }

    // Runs the samples through the decimation exactly as the connect plugin does
    std::vector<EngineData> decimate(const std::vector<EngineData> &samples)
    {
        std::vector<EngineData> kept;
        for (const EngineData &sample : samples) {
            if (kept.empty() || EngineDecimation::shouldStore(kept.back(), sample)) {
                kept.push_back(sample);
            }
        }
        return kept;
    }

    // What a replay would show: the kept samples interpolated linearly at every original timestamp
    float worstRpmDeviation(const std::vector<EngineData> &original, const std::vector<EngineData> &kept)
    {
        float worst {0.0f};
        std::size_t segment {0};
        for (const EngineData &sample : original) {
            while (segment + 2 < kept.size() && kept[segment + 1].timestamp <= sample.timestamp) {
                ++segment;
            }
            const EngineData &from = kept[segment];
            const EngineData &to = kept[std::min(segment + 1, kept.size() - 1)];
            const auto span = to.timestamp - from.timestamp;
            const float t = span > 0
                                ? static_cast<float>(sample.timestamp - from.timestamp) / static_cast<float>(span)
                                : 0.0f;
            const float replayed = from.generalEngineRpm1 + t * (to.generalEngineRpm1 - from.generalEngineRpm1);
            worst = std::max(worst, std::abs(replayed - sample.generalEngineRpm1));
        }
        return worst;
    }
}

void EngineDecimationTest::initTestCase() noexcept
{}

void EngineDecimationTest::cleanupTestCase() noexcept
{}

void EngineDecimationTest::dropsAnUnchangingIdle() noexcept
{
    // An aircraft sitting on the apron: nothing moves, so nothing needs storing until the
    // maximum interval says otherwise
    const EngineData stored = ::idling(0);
    const EngineData candidate = ::idling(::FrameMSec);

    QVERIFY(!EngineDecimation::shouldStore(stored, candidate));
}

void EngineDecimationTest::alwaysStoresAfterTheMaximumInterval() noexcept
{
    const EngineData stored = ::idling(0);
    EngineData candidate = ::idling(EngineDecimation::DefaultMaximumInterval);

    QVERIFY(EngineDecimation::shouldStore(stored, candidate));

    // ... and not a millisecond before
    candidate.timestamp = EngineDecimation::DefaultMaximumInterval - 1;
    QVERIFY(!EngineDecimation::shouldStore(stored, candidate));
}

void EngineDecimationTest::storesEveryDiscreteStateChange() noexcept
{
    const EngineData stored = ::idling(0);

    // The instant a starter is engaged, an engine catches or a battery is switched: a replay that
    // misses these by even a frame gets the sequence wrong
    EngineData starter = ::idling(::FrameMSec);
    starter.generalEngineStarter1 = true;
    QVERIFY(EngineDecimation::shouldStore(stored, starter));

    EngineData combustion = ::idling(::FrameMSec);
    combustion.generalEngineCombustion1 = false;
    QVERIFY(EngineDecimation::shouldStore(stored, combustion));

    EngineData battery = ::idling(::FrameMSec);
    battery.electricalMasterBattery2 = true;
    QVERIFY(EngineDecimation::shouldStore(stored, battery));

    // Also on the engines nobody watches
    EngineData fourth = ::idling(::FrameMSec);
    fourth.generalEngineCombustion4 = true;
    QVERIFY(EngineDecimation::shouldStore(stored, fourth));
}

void EngineDecimationTest::storesEveryLeverMovement() noexcept
{
    const EngineData stored = ::idling(0);

    EngineData throttle = ::idling(::FrameMSec);
    throttle.throttleLeverPosition1 = 1001;
    QVERIFY(EngineDecimation::shouldStore(stored, throttle));

    EngineData propeller = ::idling(::FrameMSec);
    propeller.propellerLeverPosition3 = 1;
    QVERIFY(EngineDecimation::shouldStore(stored, propeller));

    EngineData mixture = ::idling(::FrameMSec);
    mixture.mixtureLeverPosition1 = 99;
    QVERIFY(EngineDecimation::shouldStore(stored, mixture));

    EngineData cowlFlap = ::idling(::FrameMSec);
    cowlFlap.cowlFlapPosition2 = 50;
    QVERIFY(EngineDecimation::shouldStore(stored, cowlFlap));
}

void EngineDecimationTest::storesOnceTheEngineSpeedHasDrifted() noexcept
{
    const EngineData stored = ::idling(0);

    // Just under the threshold: not worth a row
    EngineData drifting = ::idling(::FrameMSec);
    drifting.generalEngineRpm1 = stored.generalEngineRpm1 + EngineDecimation::DefaultRpmThreshold * 0.5f;
    QVERIFY(!EngineDecimation::shouldStore(stored, drifting));

    // At the threshold: stored, in either direction
    drifting.generalEngineRpm1 = stored.generalEngineRpm1 + EngineDecimation::DefaultRpmThreshold;
    QVERIFY(EngineDecimation::shouldStore(stored, drifting));

    drifting.generalEngineRpm1 = stored.generalEngineRpm1 - EngineDecimation::DefaultRpmThreshold;
    QVERIFY(EngineDecimation::shouldStore(stored, drifting));
}

void EngineDecimationTest::storesOnceTheTurbineSpoolHasDrifted() noexcept
{
    const EngineData stored = ::idling(0);

    EngineData drifting = ::idling(::FrameMSec);
    drifting.turbineEngineN1Percent1 = stored.turbineEngineN1Percent1 + EngineDecimation::DefaultN1Threshold * 0.5f;
    QVERIFY(!EngineDecimation::shouldStore(stored, drifting));

    drifting.turbineEngineN1Percent1 = stored.turbineEngineN1Percent1 + EngineDecimation::DefaultN1Threshold;
    QVERIFY(EngineDecimation::shouldStore(stored, drifting));
}

void EngineDecimationTest::storesASampleThatGoesBackInTime() noexcept
{
    // A seek or a restarted recording, not a sample to judge on its contents
    const EngineData stored = ::idling(5000);

    QVERIFY(EngineDecimation::shouldStore(stored, ::idling(0)));
    QVERIFY(EngineDecimation::shouldStore(stored, ::idling(5000)));
}

void EngineDecimationTest::decimatedSpoolUpStaysWithinThreshold() noexcept
{
    // A take-off spool-up: idle to full power over eight seconds, recorded at frame rate
    std::vector<EngineData> original;
    constexpr int Frames {8 * 60};
    for (int i = 0; i < Frames; ++i) {
        const auto timestamp = i * ::FrameMSec;
        const float fraction = static_cast<float>(i) / static_cast<float>(Frames - 1);
        EngineData engine = ::idling(timestamp);
        engine.generalEngineRpm1 = 800.0f + fraction * (2400.0f - 800.0f);
        engine.turbineEngineN1Percent1 = 22.0f + fraction * (95.0f - 22.0f);
        original.push_back(engine);
    }

    const auto kept = ::decimate(original);

    // Nothing was thrown away that a gauge or an ear could tell was missing
    QVERIFY(::worstRpmDeviation(original, kept) < EngineDecimation::DefaultRpmThreshold);
    // ... and it still cost far less than storing every frame
    QVERIFY(kept.size() < original.size());
}

void EngineDecimationTest::aSteadyCruiseCostsOneSamplePerSecond() noexcept
{
    // Ten minutes of cruise with the engines settled: only the noise floor moves, which is below
    // the threshold, so the maximum interval is the only thing that stores anything
    std::vector<EngineData> original;
    constexpr int Frames {10 * 60 * 60};
    for (int i = 0; i < Frames; ++i) {
        EngineData engine = ::idling(i * ::FrameMSec);
        engine.generalEngineRpm1 = 2400.0f + (i % 2 == 0 ? 0.5f : -0.5f);
        engine.turbineEngineN1Percent1 = 88.0f;
        original.push_back(engine);
    }

    const auto kept = ::decimate(original);

    // One per second, give or take the first sample and the rounding of a frame onto a second
    const auto seconds = static_cast<std::size_t>(Frames * ::FrameMSec / EngineDecimation::DefaultMaximumInterval);
    QVERIFY(kept.size() <= seconds + 2);
    // Which is a fraction of a percent of what arrived
    QVERIFY(kept.size() * 50 < original.size());
}

QTEST_MAIN(EngineDecimationTest)
