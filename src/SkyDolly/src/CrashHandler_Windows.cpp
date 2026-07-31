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
#include <cstdlib>
#include <cstdint>
#include <new>

#include <windows.h>
#include <dbghelp.h>

#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QString>
#include <QTextStream>

#include <Kernel/Log.h>
#include <Kernel/StackTrace.h>
#include <Kernel/Version.h>
#include "CrashHandler.h"

namespace
{
    // Guards against re-entering the handler: writing the report may itself fault
    LONG volatile Handling {0};

    QString reportBaseName() noexcept
    {
        return QStringLiteral("SkyDolly-crash-%1")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-hhmmss")));
    }

    void writeMiniDump(const QString &filePath, ::EXCEPTION_POINTERS *exceptionPointers) noexcept
    {
        const HANDLE file = ::CreateFileW(reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(filePath).utf16()),
                                          GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                          FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) {
            return;
        }

        ::MINIDUMP_EXCEPTION_INFORMATION information {};
        information.ThreadId = ::GetCurrentThreadId();
        information.ExceptionPointers = exceptionPointers;
        information.ClientPointers = FALSE;

        // "WithIndirectlyReferencedMemory" keeps the dump small while still capturing the memory
        // the faulting stack points at, which is what makes it possible to see the offending
        // object in a debugger.
        const auto type = static_cast<::MINIDUMP_TYPE>(MiniDumpWithIndirectlyReferencedMemory |
                                                       MiniDumpScanMemory);
        ::MiniDumpWriteDump(::GetCurrentProcess(), ::GetCurrentProcessId(), file, type,
                            exceptionPointers != nullptr ? &information : nullptr, nullptr, nullptr);
        ::CloseHandle(file);
    }

    void writeTextReport(const QString &filePath, const QString &reason, const QString &stackTrace) noexcept
    {
        QFile file {filePath};
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            return;
        }
        QTextStream stream {&file};
        stream << "Sky Dolly abnormal termination\n"
               << "==============================\n\n"
               << "Time:    " << QDateTime::currentDateTimeUtc().toString(Qt::ISODate) << " UTC\n"
               << "Version: " << Version::getApplicationVersion() << " (" << Version::getGitHash() << ")\n"
               << "Reason:  " << reason << "\n"
               << "Log:     " << Log::getLogFilePath() << "\n\n"
               << "Stack trace\n"
               << "-----------\n"
               << stackTrace << "\n";
        stream.flush();
        file.close();
    }

    void writeReport(const QString &reason, ::EXCEPTION_POINTERS *exceptionPointers) noexcept
    {
        const QString directoryPath = CrashHandler::getCrashDirectoryPath();
        if (directoryPath.isEmpty()) {
            return;
        }
        const QDir directory {directoryPath};
        const QString baseName = ::reportBaseName();

        // The minidump is written first: it is the artefact that survives even when generating the
        // symbolised stack trace below fails (which it may, in the very state that caused this).
        ::writeMiniDump(directory.absoluteFilePath(baseName + QStringLiteral(".dmp")), exceptionPointers);

        QString stackTrace;
        try {
            stackTrace = StackTrace::generate();
        } catch (...) {
            stackTrace = QStringLiteral("<stack trace unavailable>");
        }
        ::writeTextReport(directory.absoluteFilePath(baseName + QStringLiteral(".txt")), reason, stackTrace);
    }

    LONG WINAPI unhandledExceptionFilter(::EXCEPTION_POINTERS *exceptionPointers) noexcept
    {
        if (::InterlockedExchange(&::Handling, 1) != 0) {
            return EXCEPTION_EXECUTE_HANDLER;
        }
        const auto code = exceptionPointers != nullptr && exceptionPointers->ExceptionRecord != nullptr
                              ? exceptionPointers->ExceptionRecord->ExceptionCode
                              : 0;
        ::writeReport(QStringLiteral("Unhandled structured exception, code 0x%1")
                          .arg(static_cast<quint32>(code), 8, 16, QLatin1Char('0')),
                      exceptionPointers);
        return EXCEPTION_EXECUTE_HANDLER;
    }

#ifdef _MSC_VER
    void invalidParameterHandler(const wchar_t *, const wchar_t *, const wchar_t *,
                                 unsigned int, std::uintptr_t) noexcept
    {
        if (::InterlockedExchange(&::Handling, 1) != 0) {
            return;
        }
        ::writeReport(QStringLiteral("Invalid parameter passed to a CRT function"), nullptr);
        // Terminate without unwinding: the process state is already inconsistent, and running
        // destructors from here tends to produce a second, more confusing failure.
        ::TerminateProcess(::GetCurrentProcess(), EXIT_FAILURE);
    }

    void pureCallHandler() noexcept
    {
        if (::InterlockedExchange(&::Handling, 1) != 0) {
            return;
        }
        ::writeReport(QStringLiteral("Pure virtual function called"), nullptr);
        // Terminate without unwinding: the process state is already inconsistent, and running
        // destructors from here tends to produce a second, more confusing failure.
        ::TerminateProcess(::GetCurrentProcess(), EXIT_FAILURE);
    }
#endif // _MSC_VER
}

// PUBLIC

void CrashHandler::install() noexcept
{
    ::SetUnhandledExceptionFilter(::unhandledExceptionFilter);
#ifdef _MSC_VER
    // Microsoft CRT specific: MinGW provides neither hook
    ::_set_invalid_parameter_handler(::invalidParameterHandler);
    ::_set_purecall_handler(::pureCallHandler);
#endif
}

QString CrashHandler::getCrashDirectoryPath() noexcept
{
    const QString basePath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (basePath.isEmpty()) {
        return {};
    }
    QDir directory {basePath};
    if (!directory.mkpath(QStringLiteral("crash"))) {
        return {};
    }
    return directory.absoluteFilePath(QStringLiteral("crash"));
}
