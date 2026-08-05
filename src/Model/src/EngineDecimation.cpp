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
#include <cstdint>
#include <cmath>

#include "EngineData.h"
#include "EngineDecimation.h"

namespace
{
    inline bool hasMoved(float previous, float current, float threshold) noexcept
    {
        return std::abs(current - previous) >= threshold;
    }

    //! Anything the simulator switches on or off: a replay has to reproduce these at the right instant
    inline bool discreteStateChanged(const EngineData &previous, const EngineData &current) noexcept
    {
        return previous.electricalMasterBattery1 != current.electricalMasterBattery1 ||
               previous.electricalMasterBattery2 != current.electricalMasterBattery2 ||
               previous.electricalMasterBattery3 != current.electricalMasterBattery3 ||
               previous.electricalMasterBattery4 != current.electricalMasterBattery4 ||
               previous.generalEngineStarter1 != current.generalEngineStarter1 ||
               previous.generalEngineStarter2 != current.generalEngineStarter2 ||
               previous.generalEngineStarter3 != current.generalEngineStarter3 ||
               previous.generalEngineStarter4 != current.generalEngineStarter4 ||
               previous.generalEngineCombustion1 != current.generalEngineCombustion1 ||
               previous.generalEngineCombustion2 != current.generalEngineCombustion2 ||
               previous.generalEngineCombustion3 != current.generalEngineCombustion3 ||
               previous.generalEngineCombustion4 != current.generalEngineCombustion4;
    }

    //! The levers are already quantised, so any change at all is a real one and worth a row
    inline bool leverMoved(const EngineData &previous, const EngineData &current) noexcept
    {
        return previous.throttleLeverPosition1 != current.throttleLeverPosition1 ||
               previous.throttleLeverPosition2 != current.throttleLeverPosition2 ||
               previous.throttleLeverPosition3 != current.throttleLeverPosition3 ||
               previous.throttleLeverPosition4 != current.throttleLeverPosition4 ||
               previous.propellerLeverPosition1 != current.propellerLeverPosition1 ||
               previous.propellerLeverPosition2 != current.propellerLeverPosition2 ||
               previous.propellerLeverPosition3 != current.propellerLeverPosition3 ||
               previous.propellerLeverPosition4 != current.propellerLeverPosition4 ||
               previous.mixtureLeverPosition1 != current.mixtureLeverPosition1 ||
               previous.mixtureLeverPosition2 != current.mixtureLeverPosition2 ||
               previous.mixtureLeverPosition3 != current.mixtureLeverPosition3 ||
               previous.mixtureLeverPosition4 != current.mixtureLeverPosition4 ||
               previous.cowlFlapPosition1 != current.cowlFlapPosition1 ||
               previous.cowlFlapPosition2 != current.cowlFlapPosition2 ||
               previous.cowlFlapPosition3 != current.cowlFlapPosition3 ||
               previous.cowlFlapPosition4 != current.cowlFlapPosition4;
    }

    inline bool engineSpeedMoved(const EngineData &previous, const EngineData &current,
                                 float rpmThreshold, float n1Threshold) noexcept
    {
        return hasMoved(previous.generalEngineRpm1, current.generalEngineRpm1, rpmThreshold) ||
               hasMoved(previous.generalEngineRpm2, current.generalEngineRpm2, rpmThreshold) ||
               hasMoved(previous.generalEngineRpm3, current.generalEngineRpm3, rpmThreshold) ||
               hasMoved(previous.generalEngineRpm4, current.generalEngineRpm4, rpmThreshold) ||
               hasMoved(previous.turbineEngineN1Percent1, current.turbineEngineN1Percent1, n1Threshold) ||
               hasMoved(previous.turbineEngineN1Percent2, current.turbineEngineN1Percent2, n1Threshold) ||
               hasMoved(previous.turbineEngineN1Percent3, current.turbineEngineN1Percent3, n1Threshold) ||
               hasMoved(previous.turbineEngineN1Percent4, current.turbineEngineN1Percent4, n1Threshold);
    }
}

// PUBLIC

bool EngineDecimation::shouldStore(const EngineData &lastStored, const EngineData &candidate,
                                   float rpmThreshold, float n1Threshold,
                                   std::int64_t maximumInterval) noexcept
{
    // Never drop a sample that goes back in time or repeats a timestamp: that is a seek or a
    // restarted recording, not a sample worth judging on its contents
    if (candidate.timestamp <= lastStored.timestamp) {
        return true;
    }
    if (candidate.timestamp - lastStored.timestamp >= maximumInterval) {
        return true;
    }
    return discreteStateChanged(lastStored, candidate) ||
           leverMoved(lastStored, candidate) ||
           engineSpeedMoved(lastStored, candidate, rpmThreshold, n1Threshold);
}
