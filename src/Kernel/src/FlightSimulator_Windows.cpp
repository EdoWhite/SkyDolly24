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
#include <windows.h>
#include <tchar.h>
#include <psapi.h>

#include <QDir>
#include <QStandardPaths>
#include <QString>
#include <QStringView>
#include <QStringList>

#include "FlightSimulator.h"

// To ensure correct resolution of symbols, add Psapi.lib to TARGETLIBS
// and compile with -DPSAPI_VERSION=1
static bool isProcessRunning(DWORD pid, QStringView processName)
{
    TCHAR actualProcessName[MAX_PATH] {TEXT("<unknown>")};

    // Get a handle to the process
    HANDLE processHandle = ::OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);

    // Get the process name
    if (processHandle != nullptr) {
        HMODULE moduleHandle {nullptr};
        DWORD actualSize {0};
        if (EnumProcessModules(processHandle, &moduleHandle, sizeof(moduleHandle), &actualSize)) {
            GetModuleBaseName(processHandle, moduleHandle, actualProcessName, sizeof(actualProcessName) / sizeof(TCHAR));
        }
    }

#ifdef UNICODE
    QString actualName = QString::fromWCharArray(actualProcessName);
#else
    QString actualName = QString::fromLocal8Bit(actualProcessName);
#endif

    if (processHandle != nullptr) {
        // Release the handle to the process
        CloseHandle(processHandle);
    }

    return actualName == processName;
}

// PUBLIC

bool FlightSimulator::isRunning(Id id) noexcept
{
    bool running {false};

    QString processName;
    switch (id) {
    case Id::MSFS:
        processName = "FlightSimulator.exe";
        break;
    case Id::MSFS2024:
        // Both the Steam and the MS Store edition of MSFS 2024 run under this name; the 2020
        // executable keeps the unsuffixed "FlightSimulator.exe", so the two never collide.
        processName = "FlightSimulator2024.exe";
        break;
    case Id::Prepar3Dv5:
        processName = "Prepar3D.exe";
        break;
    case Id::All:
        [[fallthrough]];
    case Id::None:
        processName = "";
        break;
    }

    if (!processName.isEmpty()) {
        // https://docs.microsoft.com/en-us/windows/win32/psapi/enumerating-all-processes
        // Get the list of process identifiers
        DWORD pidTable[1024], actualSize {0}, nofPIds {0};
        if (EnumProcesses(pidTable, sizeof(pidTable), &actualSize)) {
            // Calculate how many process identifiers were returned
            nofPIds = actualSize / sizeof(DWORD);

            // Check if process given by its name is running
            running = false;
            for (unsigned int i = 0; i < nofPIds && !running; ++i) {
                if( pidTable[i] != 0) {
                    running = isProcessRunning(pidTable[i], processName);
                }
            }
        }
    }
    return running;
}

namespace
{
    // Every simulator keeps its per-user data - UserCfg.opt, the Community folder, the package
    // cache - below one of these directories, one per edition. Their presence is what "installed"
    // is inferred from: the installation directory itself is chosen by the user (Steam library,
    // WindowsApps, a separate drive) and is not recorded anywhere we can read reliably.
    //
    // %APPDATA% already points at .../AppData/Roaming, so the MS Store paths - which live under
    // .../AppData/Local - have to be built from %LOCALAPPDATA% instead.
    bool anyDirectoryExists(const QStringList &paths) noexcept
    {
        for (const QString &path : paths) {
            if (!path.isEmpty() && QDir(path).exists()) {
                return true;
            }
        }
        return false;
    }

    QString roamingAppDataPath() noexcept
    {
        return QString::fromLocal8Bit(qgetenv("APPDATA"));
    }

    QString localAppDataPath() noexcept
    {
        return QString::fromLocal8Bit(qgetenv("LOCALAPPDATA"));
    }

    bool isMsfs2020Installed() noexcept
    {
        return anyDirectoryExists({
            // Steam edition
            roamingAppDataPath() + "/Microsoft Flight Simulator",
            // MS Store edition
            localAppDataPath() + "/Packages/Microsoft.FlightSimulator_8wekyb3d8bbwe/LocalCache"
        });
    }

    bool isMsfs2024Installed() noexcept
    {
        return anyDirectoryExists({
            // Steam edition
            roamingAppDataPath() + "/Microsoft Flight Simulator 2024",
            // MS Store edition ("Limitless" is the store package name of MSFS 2024)
            localAppDataPath() + "/Packages/Microsoft.Limitless_8wekyb3d8bbwe/LocalCache"
        });
    }

    bool isPrepar3Dv5Installed() noexcept
    {
        return anyDirectoryExists({
            QString::fromLocal8Bit(qgetenv("PROGRAMDATA")) + "/Lockheed Martin/Prepar3D v5"
        });
    }
}

bool FlightSimulator::isInstalled(Id id) noexcept
{
    // The previous implementation ignored its argument altogether and always answered for MSFS
    // 2020, using a path that could not exist (%APPDATA% already ends in "Roaming", so
    // "%APPDATA%/Local/Packages/..." never resolves). It therefore reported "not installed" on
    // every machine that only has MSFS 2024, which is precisely the target of this fork.
    bool installed {false};
    switch (id) {
    case Id::MSFS:
        installed = isMsfs2020Installed();
        break;
    case Id::MSFS2024:
        installed = isMsfs2024Installed();
        break;
    case Id::Prepar3Dv5:
        installed = isPrepar3Dv5Installed();
        break;
    case Id::All:
        // A plugin that works with any simulator is usable as soon as one of them is present
        installed = isMsfs2024Installed() || isMsfs2020Installed() || isPrepar3Dv5Installed();
        break;
    case Id::None:
        installed = false;
        break;
    }
    return installed;
}
