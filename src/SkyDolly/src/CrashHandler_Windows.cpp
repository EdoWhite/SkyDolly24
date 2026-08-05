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
#include <array>
#include <new>

#include <windows.h>
#include <dbghelp.h>

#include <QByteArray>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
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

    // Recorded when the handler is installed, which happens on the thread that runs the event loop.
    // A fault on any other thread did not come from Sky Dolly's own work
    DWORD MainThreadId {0};

    // Enough for any path the loader can hand back, MAX_PATH included
    constexpr DWORD MaximumPathLength {1024};

    QString reportBaseName() noexcept
    {
        return QStringLiteral("SkyDolly-crash-%1")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-hhmmss")));
    }

    QString hex(std::uintptr_t value, int width = 16) noexcept
    {
        return QStringLiteral("0x%1").arg(value, width, 16, QLatin1Char('0'));
    }

    /*!
     * Returns the full path of the loaded module containing \p address, or an empty string if no
     * loaded module does. An empty result is itself a diagnosis: the address is either a stale
     * function pointer or belongs to a DLL that has since been unloaded.
     */
    QString modulePathAt(std::uintptr_t address, std::uintptr_t &moduleBase) noexcept
    {
        moduleBase = 0;
        ::HMODULE module {nullptr};
        if (::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                 reinterpret_cast<LPCWSTR>(address), &module) == 0 || module == nullptr) {
            return {};
        }
        std::array<wchar_t, MaximumPathLength> path {};
        const DWORD length = ::GetModuleFileNameW(module, path.data(), MaximumPathLength);
        if (length == 0) {
            return {};
        }
        moduleBase = reinterpret_cast<std::uintptr_t>(module);
        return QString::fromWCharArray(path.data(), static_cast<int>(length));
    }

    //! Formats \p address as "module.dll+0x1234", the form that can be looked up in a disassembly
    QString describeAddress(std::uintptr_t address) noexcept
    {
        std::uintptr_t moduleBase {0};
        const QString path = modulePathAt(address, moduleBase);
        if (path.isEmpty()) {
            return hex(address) + QStringLiteral(" <no loaded module>");
        }
        return hex(address) + QStringLiteral(" %1+0x%2")
                                  .arg(QFileInfo(path).fileName())
                                  .arg(address - moduleBase, 0, 16);
    }

    QString exceptionCodeName(DWORD code) noexcept
    {
        switch (code) {
        case EXCEPTION_ACCESS_VIOLATION: return QStringLiteral("EXCEPTION_ACCESS_VIOLATION");
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return QStringLiteral("EXCEPTION_ARRAY_BOUNDS_EXCEEDED");
        case EXCEPTION_DATATYPE_MISALIGNMENT: return QStringLiteral("EXCEPTION_DATATYPE_MISALIGNMENT");
        case EXCEPTION_FLT_DIVIDE_BY_ZERO: return QStringLiteral("EXCEPTION_FLT_DIVIDE_BY_ZERO");
        case EXCEPTION_ILLEGAL_INSTRUCTION: return QStringLiteral("EXCEPTION_ILLEGAL_INSTRUCTION");
        case EXCEPTION_INT_DIVIDE_BY_ZERO: return QStringLiteral("EXCEPTION_INT_DIVIDE_BY_ZERO");
        case EXCEPTION_IN_PAGE_ERROR: return QStringLiteral("EXCEPTION_IN_PAGE_ERROR");
        case EXCEPTION_PRIV_INSTRUCTION: return QStringLiteral("EXCEPTION_PRIV_INSTRUCTION");
        case EXCEPTION_STACK_OVERFLOW: return QStringLiteral("EXCEPTION_STACK_OVERFLOW");
        case 0xC0000374: return QStringLiteral("STATUS_HEAP_CORRUPTION");
        case 0xE06D7363: return QStringLiteral("C++ exception (unhandled)");
        default: return QStringLiteral("<unknown>");
        }
    }

    /*!
     * Describes what actually failed: the faulting instruction, which module owns it, and - for an
     * access violation - what it was trying to do and to which address. This is the part of the
     * report that says whose defect it is.
     */
    QString describeFault(const ::EXCEPTION_RECORD *record) noexcept
    {
        QString text;
        QTextStream out {&text};
        const auto faultAddress = reinterpret_cast<std::uintptr_t>(record->ExceptionAddress);

        out << "Code:            " << hex(record->ExceptionCode, 8) << " "
            << exceptionCodeName(record->ExceptionCode) << "\n"
            << "Faulting address " << describeAddress(faultAddress) << "\n";

        if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record->NumberParameters >= 2) {
            const auto operation = record->ExceptionInformation[0];
            const auto target = static_cast<std::uintptr_t>(record->ExceptionInformation[1]);
            const QString what = operation == 0 ? QStringLiteral("reading")
                               : operation == 1 ? QStringLiteral("writing")
                               : operation == 8 ? QStringLiteral("executing")
                                                : QStringLiteral("accessing");
            out << "Access:          " << what << " " << hex(target) << "\n";
            if (operation == 8) {
                out << "                 (an attempt to execute code at an address that is not "
                       "executable memory)\n";
            }
        }

        std::uintptr_t moduleBase {0};
        const QString path = modulePathAt(faultAddress, moduleBase);
        if (path.isEmpty()) {
            out << "Faulting module: none - the faulting address is inside no loaded module.\n"
                   "                 Either a stale function pointer, or a DLL that was unloaded\n"
                   "                 without cancelling a callback it had registered.\n";
        } else {
            out << "Faulting module: " << path << "\n";
            const QString applicationDirectory = QCoreApplication::applicationDirPath();
            if (!applicationDirectory.isEmpty() &&
                !QFileInfo(path).absoluteFilePath().startsWith(applicationDirectory, Qt::CaseInsensitive)) {
                out << "                 (not part of the Sky Dolly installation)\n";
            }
        }

        const DWORD threadId = ::GetCurrentThreadId();
        out << "Faulting thread: " << threadId
            << (threadId == MainThreadId ? " (the main thread)" : " (not the main thread)") << "\n";
        return text;
    }

    /*!
     * Walks the stack of the thread that faulted, starting from the context captured at the fault.
     *
     * This is deliberately not cpptrace's generate(): that one walks the stack of whoever calls it,
     * which here is the exception filter, and so reports the handler's own frames rather than the
     * ones that led to the fault.
     */
    QString faultingStackTrace(const ::CONTEXT *contextRecord) noexcept
    {
        // StackWalk64 writes through the context it is given
        ::CONTEXT context = *contextRecord;
        const HANDLE process = ::GetCurrentProcess();
        const HANDLE thread = ::GetCurrentThread();

        ::SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME | SYMOPT_LOAD_LINES | SYMOPT_FAIL_CRITICAL_ERRORS);
        const bool symbolsInitialised = ::SymInitialize(process, nullptr, TRUE) != FALSE;

        ::STACKFRAME64 frame {};
#if defined(_M_X64)
        constexpr DWORD machine {IMAGE_FILE_MACHINE_AMD64};
        frame.AddrPC.Offset = context.Rip;
        frame.AddrFrame.Offset = context.Rbp;
        frame.AddrStack.Offset = context.Rsp;
#elif defined(_M_ARM64)
        constexpr DWORD machine {IMAGE_FILE_MACHINE_ARM64};
        frame.AddrPC.Offset = context.Pc;
        frame.AddrFrame.Offset = context.Fp;
        frame.AddrStack.Offset = context.Sp;
#else
        constexpr DWORD machine {IMAGE_FILE_MACHINE_I386};
        frame.AddrPC.Offset = context.Eip;
        frame.AddrFrame.Offset = context.Ebp;
        frame.AddrStack.Offset = context.Esp;
#endif
        frame.AddrPC.Mode = AddrModeFlat;
        frame.AddrFrame.Mode = AddrModeFlat;
        frame.AddrStack.Mode = AddrModeFlat;

        QString text;
        QTextStream out {&text};
        out << "Stack trace of the faulting thread (most recent call first):\n";

        // The frame at the fault itself may be unwalkable - an invalid instruction pointer has no
        // unwind data - in which case StackWalk64 falls back to treating it as a leaf function and
        // takes the return address straight off the stack, which is exactly the caller wanted here
        constexpr int MaximumFrames {64};
        int counter {0};
        for (; counter < MaximumFrames; ++counter) {
            if (::StackWalk64(machine, process, thread, &frame, &context, nullptr,
                              ::SymFunctionTableAccess64, ::SymGetModuleBase64, nullptr) == FALSE) {
                break;
            }
            const auto address = static_cast<std::uintptr_t>(frame.AddrPC.Offset);
            if (address == 0) {
                break;
            }
            out << "#" << counter << " " << describeAddress(address);

            if (symbolsInitialised) {
                // The wide variants throughout, spelled out rather than left to the UNICODE
                // macros: SYMBOL_INFO maps to SYMBOL_INFOW but IMAGEHLP_LINE64 does not map to its
                // wide twin, so the two names in the same call would disagree about their encoding
                // and every symbol would come out as its first letter alone
                std::array<char, sizeof(::SYMBOL_INFOW) + MAX_SYM_NAME * sizeof(wchar_t)> storage {};
                auto *symbol = reinterpret_cast<::SYMBOL_INFOW *>(storage.data());
                symbol->SizeOfStruct = sizeof(::SYMBOL_INFOW);
                symbol->MaxNameLen = MAX_SYM_NAME;
                DWORD64 displacement {0};
                if (::SymFromAddrW(process, frame.AddrPC.Offset, &displacement, symbol) != FALSE) {
                    out << " in " << QString::fromWCharArray(symbol->Name);
                    if (displacement != 0) {
                        out << "+0x" << QString::number(displacement, 16);
                    }
                    ::IMAGEHLP_LINEW64 line {};
                    line.SizeOfStruct = sizeof(::IMAGEHLP_LINEW64);
                    DWORD lineDisplacement {0};
                    if (::SymGetLineFromAddrW64(process, frame.AddrPC.Offset, &lineDisplacement, &line) != FALSE) {
                        out << " at " << QString::fromWCharArray(line.FileName) << ":" << line.LineNumber;
                    }
                }
            }
            out << "\n";
        }
        if (counter == 0) {
            out << "<the stack could not be walked>\n";
        }
        if (symbolsInitialised) {
            ::SymCleanup(process);
        }
        return text;
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

    void writeTextReport(const QString &filePath, const QString &reason, const QString &fault,
                         const QString &stackTrace) noexcept
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
               << "Log:     " << Log::getLogFilePath() << "\n\n";
        if (!fault.isEmpty()) {
            stream << "Fault\n"
                   << "-----\n"
                   << fault << "\n";
        }
        stream << "Stack trace\n"
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

        QString fault;
        QString stackTrace;
        const ::EXCEPTION_RECORD *record = exceptionPointers != nullptr ? exceptionPointers->ExceptionRecord : nullptr;
        const ::CONTEXT *context = exceptionPointers != nullptr ? exceptionPointers->ContextRecord : nullptr;
        try {
            if (record != nullptr) {
                fault = ::describeFault(record);
            }
            if (context != nullptr) {
                // Walk the thread that faulted, from the context captured at the fault
                stackTrace = ::faultingStackTrace(context);
            } else {
                // No context: the CRT hooks below are called from the failing code itself, so the
                // handler's own stack is the interesting one
                stackTrace = StackTrace::generate();
            }
        } catch (...) {
            stackTrace = QStringLiteral("<stack trace unavailable>");
        }
        ::writeTextReport(directory.absoluteFilePath(baseName + QStringLiteral(".txt")), reason, fault, stackTrace);
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
    ::MainThreadId = ::GetCurrentThreadId();
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
