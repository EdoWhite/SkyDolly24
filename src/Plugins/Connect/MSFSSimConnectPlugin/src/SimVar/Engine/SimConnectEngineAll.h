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
#ifndef SIMCONNECTENGINEALL_H
#define SIMCONNECTENGINEALL_H

#include <Kernel/Enum.h>
#include <Model/EngineData.h>
#include "SimConnectType.h"
#include "SimConnectEngineCommon.h"
#include "SimConnectEngineCore.h"
#include "SimConnectEngineEvent.h"
#include "SimConnectEngineInfo.h"
#include "SimConnectEngineUser.h"
#include "SimConnectEngineAi.h"

/*!
 * All engine simulation variables (reply from the flight simulator).
 *
 * Implementation note: this struct needs to be packed.
 */
#pragma pack(push, 1)
struct SimConnectEngineAll
{
    SimConnectEngineCommon common;
    SimConnectEngineCore core;
    SimConnectEngineEvent event;
    // Last, and it has to stay last: the memory layout of this struct is the layout of the data
    // definition below, so the order of the members and the order of the addToDataDefinition calls
    // are the same list written twice
    SimConnectEngineInfo info;

    SimConnectEngineAll(const EngineData &data) noexcept
        : SimConnectEngineAll()
    {
        fromEngineData(data);
    }

    SimConnectEngineAll() = default;

    inline void fromEngineData(const EngineData &data) noexcept
    {
        common.fromEngineData(data);
        core.fromEngineData(data);
        event.fromEngineData(data);
        info.fromEngineData(data);
    }

    inline EngineData toEngineData() const noexcept
    {
        EngineData data = common.toEngineData();
        core.toEngineData(data);
        event.toEngineData(data);
        info.toEngineData(data);
        return data;
    }

    inline SimConnectEngineUser user() const noexcept
    {
        SimConnectEngineUser user;
        user.common = common;
        user.core = core;
        return user;
    }

    inline SimConnectEngineAi ai() const noexcept
    {
        SimConnectEngineAi ai;
        ai.common = common;
        ai.event = event;
        return ai;
    }

    static void addToDataDefinition(HANDLE simConnectHandle) noexcept
    {
        SimConnectEngineCommon::addToDataDefinition(simConnectHandle, Enum::underly(SimConnectType::DataDefinition::EngineAll));
        SimConnectEngineCore::addToDataDefinition(simConnectHandle, Enum::underly(SimConnectType::DataDefinition::EngineAll));
        SimConnectEngineEvent::addToDataDefinition(simConnectHandle, Enum::underly(SimConnectType::DataDefinition::EngineAll));
        // Recorded only: neither user() nor ai() above carries it, so nothing here is ever sent
        // back to the simulator
        SimConnectEngineInfo::addToDataDefinition(simConnectHandle, Enum::underly(SimConnectType::DataDefinition::EngineAll));
    }
};
#pragma pack(pop)

#endif // SIMCONNECTENGINEALL_H
