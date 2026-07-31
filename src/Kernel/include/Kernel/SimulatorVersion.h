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
#ifndef SIMULATORVERSION_H
#define SIMULATORVERSION_H

#include <cstdint>

#include <QString>

#include "FlightSimulator.h"
#include "KernelLib.h"

/*!
 * Identifies the simulator a connect plugin is actually talking to, as reported by the simulator
 * itself when the connection is opened.
 *
 * This matters because the same client works with more than one simulator generation while their
 * behaviour differs: which system events are sent, how pause is reported, whether a teleport is
 * safe. Compile-time knowledge is not enough - the SDK a build was compiled against says nothing
 * about what is running on the other end - so the few places that must behave differently branch
 * on this instead of on assumptions.
 *
 * All values are zero / empty until a connection has been established at least once.
 */
struct KERNEL_API SimulatorVersion
{
    /*!
     * The name the simulator reports for itself. These are internal code names rather than
     * marketing names:
     *
     * - \c KittyHawk - Microsoft Flight Simulator 2020
     * - \c SunRise - Microsoft Flight Simulator 2024 (observed: application version 12.2,
     *   SimConnect version 12.2)
     */
    QString applicationName;
    std::uint32_t applicationVersionMajor {0};
    std::uint32_t applicationVersionMinor {0};
    std::uint32_t applicationBuildMajor {0};
    std::uint32_t applicationBuildMinor {0};
    std::uint32_t simConnectVersionMajor {0};
    std::uint32_t simConnectVersionMinor {0};

    static constexpr const char *ApplicationNameMSFS {"KittyHawk"};
    static constexpr const char *ApplicationNameMSFS2024 {"SunRise"};

    /*!
     * Returns whether a simulator has identified itself, that is whether a connection has been
     * established at least once.
     */
    bool isValid() const noexcept;

    /*!
     * Returns which simulator is on the other end of the connection.
     *
     * The reported application name is authoritative; the application version is used as a
     * fallback for a future release that renames itself again, given that MSFS 2020 never went
     * beyond major version 11.
     */
    FlightSimulator::Id getFlightSimulatorId() const noexcept;

    /*!
     * Returns whether the connected simulator is Microsoft Flight Simulator 2024.
     */
    bool isMSFS2024() const noexcept;

    /*!
     * Returns a human readable one-line description, for the about dialog and the log.
     */
    QString toString() const noexcept;
};

#endif // SIMULATORVERSION_H
