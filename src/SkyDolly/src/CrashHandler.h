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
#ifndef CRASHHANDLER_H
#define CRASHHANDLER_H

#include <QString>

/*!
 * Catches the failures that std::set_terminate cannot see.
 *
 * ExceptionHandler covers C++ exceptions, but an access violation, a stack overflow or a call
 * through a bad pointer never becomes a C++ exception on Windows: the process simply disappears.
 * Those are exactly the failures Sky Dolly suffers from when the flight simulator connection is
 * torn down at the wrong moment, and until now they left nothing behind at all.
 *
 * On platforms other than Windows this is a no-op - POSIX signals are already handled by
 * SignalHandler.
 */
namespace CrashHandler
{
    /*!
     * Installs the platform's unhandled-failure hooks. Call once, early in main().
     */
    void install() noexcept;

    /*!
     * Returns the directory that crash reports are written to, creating it if required.
     */
    QString getCrashDirectoryPath() noexcept;
}

#endif // CRASHHANDLER_H
