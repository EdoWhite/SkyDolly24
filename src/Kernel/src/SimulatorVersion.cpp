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
#include <QString>
#include <QLatin1StringView>
#include <QCoreApplication>

#include "FlightSimulator.h"
#include "SimulatorVersion.h"

namespace
{
    // MSFS 2020 shipped as application version 11.x, MSFS 2024 as 12.x. Used only when the
    // application name is not one we know.
    constexpr std::uint32_t FirstMSFS2024ApplicationVersionMajor {12};
}

// PUBLIC

bool SimulatorVersion::isValid() const noexcept
{
    return !applicationName.isEmpty();
}

FlightSimulator::Id SimulatorVersion::getFlightSimulatorId() const noexcept
{
    FlightSimulator::Id id {FlightSimulator::Id::None};
    if (applicationName == QLatin1StringView(ApplicationNameMSFS2024)) {
        id = FlightSimulator::Id::MSFS2024;
    } else if (applicationName == QLatin1StringView(ApplicationNameMSFS)) {
        id = FlightSimulator::Id::MSFS;
    } else if (isValid()) {
        // An unknown name: assume a future MSFS release and go by the application version, so that
        // a rename does not silently downgrade the behaviour to the 2020 code paths
        id = applicationVersionMajor >= FirstMSFS2024ApplicationVersionMajor ?
             FlightSimulator::Id::MSFS2024 : FlightSimulator::Id::MSFS;
    }
    return id;
}

bool SimulatorVersion::isMSFS2024() const noexcept
{
    return getFlightSimulatorId() == FlightSimulator::Id::MSFS2024;
}

QString SimulatorVersion::toString() const noexcept
{
    if (!isValid()) {
        return QCoreApplication::translate("SimulatorVersion", "Not connected");
    }

    QString name;
    switch (getFlightSimulatorId()) {
    case FlightSimulator::Id::MSFS2024:
        name = QStringLiteral("Microsoft Flight Simulator 2024");
        break;
    case FlightSimulator::Id::MSFS:
        name = QStringLiteral("Microsoft Flight Simulator 2020");
        break;
    default:
        name = applicationName;
        break;
    }

    // For example: Microsoft Flight Simulator 2024 12.2 (build 282174.999, SunRise, SimConnect 12.2)
    return QStringLiteral("%1 %2.%3 (build %4.%5, %6, SimConnect %7.%8)")
        .arg(name)
        .arg(applicationVersionMajor)
        .arg(applicationVersionMinor)
        .arg(applicationBuildMajor)
        .arg(applicationBuildMinor)
        .arg(applicationName)
        .arg(simConnectVersionMajor)
        .arg(simConnectVersionMinor);
}
