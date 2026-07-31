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
#include <mutex>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QString>
#include <QTextStream>
// Provides qInstallMessageHandler, QtMessageHandler and QMessageLogContext
#include <QDebug>

#include "Version.h"
#include "Log.h"

namespace
{
    // Log files older than this are removed on startup
    constexpr int MaxLogFileCount {10};

    std::mutex LogMutex;
    QFile LogFile;
    QTextStream LogStream;
    QtMessageHandler PreviousMessageHandler {nullptr};

    constexpr const char *levelName(QtMsgType type) noexcept
    {
        switch (type) {
        case QtDebugMsg: return "DEBUG";
        case QtInfoMsg: return "INFO ";
        case QtWarningMsg: return "WARN ";
        case QtCriticalMsg: return "ERROR";
        case QtFatalMsg: return "FATAL";
        }
        return "?????";
    }

    // Deliberately not noexcept: an allocation failure while logging should propagate rather than
    // call std::terminate from inside the message handler
    void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
    {
        {
            const std::lock_guard<std::mutex> lock {LogMutex};
            if (LogFile.isOpen()) {
                LogStream << QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)
                          << ' ' << ::levelName(type) << ' ' << message;
                // Only debug builds carry file/line information; in release builds Qt strips it
                if (context.file != nullptr) {
                    LogStream << " [" << context.file << ':' << context.line << ']';
                }
                LogStream << '\n';
                // Flushed on every message: the interesting messages are the ones written just
                // before a crash, and a buffered stream would lose exactly those
                LogStream.flush();
            }
        }
        if (::PreviousMessageHandler != nullptr) {
            ::PreviousMessageHandler(type, context, message);
        }
    }

    //! Keeps the most recent MaxLogFileCount log files and deletes the rest
    void removeStaleLogFiles(const QDir &directory)
    {
        QFileInfoList logFiles = directory.entryInfoList({"SkyDolly-*.log"}, QDir::Files, QDir::Time);
        for (int i = ::MaxLogFileCount; i < logFiles.count(); ++i) {
            QFile::remove(logFiles.at(i).absoluteFilePath());
        }
    }
}

// PUBLIC

void Log::initialise() noexcept
{
    const std::lock_guard<std::mutex> lock {::LogMutex};
    if (::LogFile.isOpen()) {
        return;
    }

    const QString directoryPath = Log::getLogDirectoryPath();
    if (directoryPath.isEmpty()) {
        return;
    }
    QDir directory {directoryPath};
    ::removeStaleLogFiles(directory);

    const QString fileName = QStringLiteral("SkyDolly-%1.log")
                                 .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-hhmmss")));
    ::LogFile.setFileName(directory.absoluteFilePath(fileName));
    if (!::LogFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return;
    }
    ::LogStream.setDevice(&::LogFile);

    ::PreviousMessageHandler = qInstallMessageHandler(::messageHandler);

    ::LogStream << "Sky Dolly " << Version::getApplicationVersion()
                << " (" << Version::getGitHash() << ") - log started "
                << QDateTime::currentDateTimeUtc().toString(Qt::ISODate) << " UTC\n";
    ::LogStream.flush();
}

void Log::shutdown() noexcept
{
    const std::lock_guard<std::mutex> lock {::LogMutex};
    if (!::LogFile.isOpen()) {
        return;
    }
    qInstallMessageHandler(::PreviousMessageHandler);
    ::PreviousMessageHandler = nullptr;
    ::LogStream.flush();
    ::LogStream.setDevice(nullptr);
    ::LogFile.close();
}

QString Log::getLogDirectoryPath() noexcept
{
    const QString basePath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (basePath.isEmpty()) {
        return {};
    }
    QDir directory {basePath};
    if (!directory.mkpath(QStringLiteral("logs"))) {
        return {};
    }
    return directory.absoluteFilePath(QStringLiteral("logs"));
}

QString Log::getLogFilePath() noexcept
{
    const std::lock_guard<std::mutex> lock {::LogMutex};
    return ::LogFile.isOpen() ? ::LogFile.fileName() : QString {};
}
