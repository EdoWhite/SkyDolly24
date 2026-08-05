/**
 * Sky Dolly - The Black Sheep for your Flight Recordings
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
#ifndef SIMCONNECTENGINEINFO_H
#define SIMCONNECTENGINEINFO_H

#include <windows.h>
#include <SimConnect.h>

#include <Model/SimVar.h>
#include <Model/EngineData.h>

/*!
 * Engine simulation variables that are stored for information purposes only.
 *
 * How fast the engines are actually turning is an *output* of the flight simulator's engine model,
 * not an input to it: the simulator works these out from the lever positions, the air around the
 * aircraft and the state the engines were already in. So they belong in an Info record - read
 * during recording, never sent back during replay - and that is why SimConnectEngineUser and
 * SimConnectEngineAi do not carry them.
 *
 * Recording them is worth doing even before anything replays them, because it is not reversible:
 * a flight recorded without these can never be replayed faithfully afterwards, however the replay
 * side eventually turns out.
 *
 * Implementation note: this struct needs to be packed.
 */
#pragma pack(push, 1)
struct SimConnectEngineInfo
{
    float generalEngineRpm1 {0.0f};
    float generalEngineRpm2 {0.0f};
    float generalEngineRpm3 {0.0f};
    float generalEngineRpm4 {0.0f};
    float turbineEngineN1Percent1 {0.0f};
    float turbineEngineN1Percent2 {0.0f};
    float turbineEngineN1Percent3 {0.0f};
    float turbineEngineN1Percent4 {0.0f};

    SimConnectEngineInfo(const EngineData &data) noexcept
        : SimConnectEngineInfo()
    {
        fromEngineData(data);
    }

    SimConnectEngineInfo() = default;

    inline void fromEngineData(const EngineData &data) noexcept
    {
        generalEngineRpm1 = data.generalEngineRpm1;
        generalEngineRpm2 = data.generalEngineRpm2;
        generalEngineRpm3 = data.generalEngineRpm3;
        generalEngineRpm4 = data.generalEngineRpm4;
        turbineEngineN1Percent1 = data.turbineEngineN1Percent1;
        turbineEngineN1Percent2 = data.turbineEngineN1Percent2;
        turbineEngineN1Percent3 = data.turbineEngineN1Percent3;
        turbineEngineN1Percent4 = data.turbineEngineN1Percent4;
    }

    inline EngineData toEngineData() const noexcept
    {
        EngineData data;
        toEngineData(data);
        return data;
    }

    inline void toEngineData(EngineData &data) const noexcept
    {
        data.generalEngineRpm1 = generalEngineRpm1;
        data.generalEngineRpm2 = generalEngineRpm2;
        data.generalEngineRpm3 = generalEngineRpm3;
        data.generalEngineRpm4 = generalEngineRpm4;
        data.turbineEngineN1Percent1 = turbineEngineN1Percent1;
        data.turbineEngineN1Percent2 = turbineEngineN1Percent2;
        data.turbineEngineN1Percent3 = turbineEngineN1Percent3;
        data.turbineEngineN1Percent4 = turbineEngineN1Percent4;
    }

    static inline void addToDataDefinition(HANDLE simConnectHandle, ::SIMCONNECT_DATA_DEFINITION_ID dataDefinitionId) noexcept
    {
        ::SimConnect_AddToDataDefinition(simConnectHandle, dataDefinitionId, SimVar::GeneralEngineRpm1, "RPM", ::SIMCONNECT_DATATYPE_FLOAT32);
        ::SimConnect_AddToDataDefinition(simConnectHandle, dataDefinitionId, SimVar::GeneralEngineRpm2, "RPM", ::SIMCONNECT_DATATYPE_FLOAT32);
        ::SimConnect_AddToDataDefinition(simConnectHandle, dataDefinitionId, SimVar::GeneralEngineRpm3, "RPM", ::SIMCONNECT_DATATYPE_FLOAT32);
        ::SimConnect_AddToDataDefinition(simConnectHandle, dataDefinitionId, SimVar::GeneralEngineRpm4, "RPM", ::SIMCONNECT_DATATYPE_FLOAT32);
        ::SimConnect_AddToDataDefinition(simConnectHandle, dataDefinitionId, SimVar::TurbineEngineN1Percent1, "Percent", ::SIMCONNECT_DATATYPE_FLOAT32);
        ::SimConnect_AddToDataDefinition(simConnectHandle, dataDefinitionId, SimVar::TurbineEngineN1Percent2, "Percent", ::SIMCONNECT_DATATYPE_FLOAT32);
        ::SimConnect_AddToDataDefinition(simConnectHandle, dataDefinitionId, SimVar::TurbineEngineN1Percent3, "Percent", ::SIMCONNECT_DATATYPE_FLOAT32);
        ::SimConnect_AddToDataDefinition(simConnectHandle, dataDefinitionId, SimVar::TurbineEngineN1Percent4, "Percent", ::SIMCONNECT_DATATYPE_FLOAT32);
    }
};
#pragma pack(pop)

#endif // SIMCONNECTENGINEINFO_H
