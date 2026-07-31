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
#ifndef SIMCONNECTERROR_H
#define SIMCONNECTERROR_H

#include <windows.h>
#include <SimConnect.h>

/*!
 * Decodes SimConnect server exceptions into readable text.
 *
 * The simulator reports rejected requests asynchronously as SIMCONNECT_RECV_ID_EXCEPTION rather
 * than as a failed return code, so this is the only channel through which "MSFS refused what we
 * just sent" becomes visible. That matters for MSFS 2024, which rejects some requests that MSFS
 * 2020 accepted.
 */
namespace SimConnectError
{
    /*!
     * Returns the symbolic name for the SimConnect exception \p exception, or \c nullptr for
     * codes that are unknown to the SDK this was compiled against.
     */
    inline const char *nameOf(DWORD exception) noexcept
    {
        switch (static_cast<::SIMCONNECT_EXCEPTION>(exception)) {
        case ::SIMCONNECT_EXCEPTION_NONE: return "NONE";
        case ::SIMCONNECT_EXCEPTION_ERROR: return "ERROR";
        case ::SIMCONNECT_EXCEPTION_SIZE_MISMATCH: return "SIZE_MISMATCH";
        case ::SIMCONNECT_EXCEPTION_UNRECOGNIZED_ID: return "UNRECOGNIZED_ID";
        case ::SIMCONNECT_EXCEPTION_UNOPENED: return "UNOPENED";
        case ::SIMCONNECT_EXCEPTION_VERSION_MISMATCH: return "VERSION_MISMATCH";
        case ::SIMCONNECT_EXCEPTION_TOO_MANY_GROUPS: return "TOO_MANY_GROUPS";
        case ::SIMCONNECT_EXCEPTION_NAME_UNRECOGNIZED: return "NAME_UNRECOGNIZED";
        case ::SIMCONNECT_EXCEPTION_TOO_MANY_EVENT_NAMES: return "TOO_MANY_EVENT_NAMES";
        case ::SIMCONNECT_EXCEPTION_EVENT_ID_DUPLICATE: return "EVENT_ID_DUPLICATE";
        case ::SIMCONNECT_EXCEPTION_TOO_MANY_MAPS: return "TOO_MANY_MAPS";
        case ::SIMCONNECT_EXCEPTION_TOO_MANY_OBJECTS: return "TOO_MANY_OBJECTS";
        case ::SIMCONNECT_EXCEPTION_TOO_MANY_REQUESTS: return "TOO_MANY_REQUESTS";
        case ::SIMCONNECT_EXCEPTION_WEATHER_INVALID_PORT: return "WEATHER_INVALID_PORT";
        case ::SIMCONNECT_EXCEPTION_WEATHER_INVALID_METAR: return "WEATHER_INVALID_METAR";
        case ::SIMCONNECT_EXCEPTION_WEATHER_UNABLE_TO_GET_OBSERVATION: return "WEATHER_UNABLE_TO_GET_OBSERVATION";
        case ::SIMCONNECT_EXCEPTION_WEATHER_UNABLE_TO_CREATE_STATION: return "WEATHER_UNABLE_TO_CREATE_STATION";
        case ::SIMCONNECT_EXCEPTION_WEATHER_UNABLE_TO_REMOVE_STATION: return "WEATHER_UNABLE_TO_REMOVE_STATION";
        case ::SIMCONNECT_EXCEPTION_INVALID_DATA_TYPE: return "INVALID_DATA_TYPE";
        case ::SIMCONNECT_EXCEPTION_INVALID_DATA_SIZE: return "INVALID_DATA_SIZE";
        case ::SIMCONNECT_EXCEPTION_DATA_ERROR: return "DATA_ERROR";
        case ::SIMCONNECT_EXCEPTION_INVALID_ARRAY: return "INVALID_ARRAY";
        case ::SIMCONNECT_EXCEPTION_CREATE_OBJECT_FAILED: return "CREATE_OBJECT_FAILED";
        case ::SIMCONNECT_EXCEPTION_LOAD_FLIGHTPLAN_FAILED: return "LOAD_FLIGHTPLAN_FAILED";
        case ::SIMCONNECT_EXCEPTION_OPERATION_INVALID_FOR_OBJECT_TYPE: return "OPERATION_INVALID_FOR_OBJECT_TYPE";
        case ::SIMCONNECT_EXCEPTION_ILLEGAL_OPERATION: return "ILLEGAL_OPERATION";
        case ::SIMCONNECT_EXCEPTION_ALREADY_SUBSCRIBED: return "ALREADY_SUBSCRIBED";
        case ::SIMCONNECT_EXCEPTION_INVALID_ENUM: return "INVALID_ENUM";
        case ::SIMCONNECT_EXCEPTION_DEFINITION_ERROR: return "DEFINITION_ERROR";
        case ::SIMCONNECT_EXCEPTION_DUPLICATE_ID: return "DUPLICATE_ID";
        case ::SIMCONNECT_EXCEPTION_DATUM_ID: return "DATUM_ID";
        case ::SIMCONNECT_EXCEPTION_OUT_OF_BOUNDS: return "OUT_OF_BOUNDS";
        case ::SIMCONNECT_EXCEPTION_ALREADY_CREATED: return "ALREADY_CREATED";
        case ::SIMCONNECT_EXCEPTION_OBJECT_OUTSIDE_REALITY_BUBBLE: return "OBJECT_OUTSIDE_REALITY_BUBBLE";
        case ::SIMCONNECT_EXCEPTION_OBJECT_CONTAINER: return "OBJECT_CONTAINER";
        case ::SIMCONNECT_EXCEPTION_OBJECT_AI: return "OBJECT_AI";
        case ::SIMCONNECT_EXCEPTION_OBJECT_ATC: return "OBJECT_ATC";
        case ::SIMCONNECT_EXCEPTION_OBJECT_SCHEDULE: return "OBJECT_SCHEDULE";
        default: return nullptr;
        }
    }

    /*!
     * Returns a short explanation of what \p exception typically means for Sky Dolly, or
     * \c nullptr when there is nothing useful to add beyond the symbolic name.
     */
    inline const char *hintFor(DWORD exception) noexcept
    {
        switch (static_cast<::SIMCONNECT_EXCEPTION>(exception)) {
        case ::SIMCONNECT_EXCEPTION_VERSION_MISMATCH:
            return "the SimConnect client library does not match the simulator - "
                   "rebuild against the MSFS 2024 SDK";
        case ::SIMCONNECT_EXCEPTION_CREATE_OBJECT_FAILED:
            return "the simulator could not spawn an AI aircraft - the recorded aircraft title "
                   "is probably not installed";
        case ::SIMCONNECT_EXCEPTION_OBJECT_OUTSIDE_REALITY_BUBBLE:
            return "the object is too far away from the user aircraft to be simulated";
        case ::SIMCONNECT_EXCEPTION_UNRECOGNIZED_ID:
        case ::SIMCONNECT_EXCEPTION_NAME_UNRECOGNIZED:
            return "a simulation variable or event is not known to this simulator version";
        case ::SIMCONNECT_EXCEPTION_OPERATION_INVALID_FOR_OBJECT_TYPE:
        case ::SIMCONNECT_EXCEPTION_ILLEGAL_OPERATION:
            return "the simulator refused to apply the data - the aircraft may be simulating "
                   "this value itself";
        default:
            return nullptr;
        }
    }
}

#endif // SIMCONNECTERROR_H
