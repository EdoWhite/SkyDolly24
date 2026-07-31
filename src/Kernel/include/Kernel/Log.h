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
#ifndef LOG_H
#define LOG_H

#include <QString>

#include "KernelLib.h"

/*!
 * Writes the application's diagnostic output to a file.
 *
 * Sky Dolly is built as a GUI application (no attached console), so without this every qDebug(),
 * qWarning() and qCritical() message is discarded unless a debugger happens to be attached. That
 * makes problems that only occur on a user's machine - a rejected SimConnect request, a connection
 * that never establishes - impossible to diagnose.
 */
class KERNEL_API Log final
{
public:
    /*!
     * Installs the message handler that writes to the log file, and starts a new log file for this
     * run. Call once, as early in main() as possible.
     *
     * Messages continue to be forwarded to the default handler, so debug output is unaffected.
     */
    static void initialise() noexcept;

    /*!
     * Flushes and closes the log file and restores the default message handler. Call before the
     * application object goes away.
     */
    static void shutdown() noexcept;

    /*!
     * Returns the directory that holds the log files, creating it if required.
     *
     * The location is the application's writable "app local data" path, which on Windows is
     * %LOCALAPPDATA%\\<organisation>\\<application>\\logs.
     */
    static QString getLogDirectoryPath() noexcept;

    /*!
     * Returns the path of the log file for the current run, or a null string when logging has not
     * been initialised.
     */
    static QString getLogFilePath() noexcept;
};

#endif // LOG_H
