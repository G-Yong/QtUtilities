/*
 * dump.h — Crash dump utilities (single-file header library, C++17 inline style)
 *
 * Requires C++17 or later (for inline variables).
 *
 * Minidump generation is only available on Windows; on other platforms all
 * functions are safe no-ops so the header can be included unconditionally.
 *
 * USAGE
 * -----
 *   #include "dump.h"
 *
 *   int main(int argc, char *argv[]) {
 *       QCoreApplication app(argc, argv);
 *
 *       QtUtil::appName    = "myapp";      // default: "DumpFile"
 *       QtUtil::appVersion = "1.0.0";      // default: "" (omitted)
 *       QtUtil::dumpDir    = "crash_dumps"; // default: "dumps"
 *       QtUtil::installCrashHandler();
 *
 *       // Optional: block CRT/JIT from overwriting your handler
 *       QtUtil::preventUnhandledExceptionFilterOverride();
 *
 *       // Your program runs here — on crash a .dmp is written to dumpDir.
 *   }
 *
 * QUICK START (minimal)
 * ---------------------
 *   QtUtil::installCrashHandler();   // that's it — defaults: appName="DumpFile", dumpDir="dumps"
 *
 * DUMP FILE LOCATION
 * ------------------
 *   By default dumps are written to "dumps/" relative to the working directory.
 *   Change at runtime before calling installCrashHandler():
 *       QtUtil::dumpDir = "crash_dumps";
 *
 * DUMP FILE NAMING
 * ----------------
 *   (appName)_(version)_yyyyMMdd_HHmmss.zzz.dmp
 *   If version is empty the version block is omitted:
 *   (appName)_yyyyMMdd_HHmmss.zzz.dmp
 *
 * HOW IT WORKS (Windows)
 * ----------------------
 * installCrashHandler() registers:
 *   1. SetUnhandledExceptionFilter → unhandledExceptionFilter()
 *        Catches native / SEH exceptions (access violation, stack overflow …)
 *   2. _set_invalid_parameter_handler → invalidParameterHandler()
 *        Catches CRT invalid-parameter calls (e.g. printf format mismatch in Debug)
 *   3. _set_purecall_handler → pureCallHandler()
 *        Catches calls to pure virtual functions
 *   4. signal(SIGABRT) → signalHandler()
 *        Catches abort() / std::terminate
 *
 * When any of these fire, generateMiniDump() uses DbgHelp!MiniDumpWriteDump
 * to write a minidump file, then terminates the process.
 *
 * preventUnhandledExceptionFilterOverride() patches kernel32!SetUnhandledExceptionFilter
 * so that the CRT / third-party DLLs cannot replace your handler after startup.
 *
 * PLATFORM NOTES
 * --------------
 * - Windows:  Full functionality. Link against DbgHelp.lib (handled via
 *             #pragma comment below) and user32.lib.
 * - Linux / macOS:  All functions compile and run as no-ops.
 * - WebAssembly:    All functions compile and run as no-ops.
 */

#ifndef DUMP_H
#define DUMP_H

#include <QString>
#include <QDir>
#include <QDateTime>
#include <QDebug>

// ---------------------------------------------------------------------------
// Platform detection
// ---------------------------------------------------------------------------
#if defined(Q_OS_WIN)
#define QTUTIL_CRASH_DUMP_SUPPORTED 1
#else
#define QTUTIL_CRASH_DUMP_SUPPORTED 0
#endif

#if QTUTIL_CRASH_DUMP_SUPPORTED
#include <csignal>

#include <Windows.h>
#include <DbgHelp.h>

// Auto-link DbgHelp on MSVC (MinGW users may need to add -lDbgHelp manually).
#ifdef _MSC_VER
#pragma comment(lib, "DbgHelp.Lib")
#pragma comment(lib, "user32.lib")
#endif

#else
#include <csignal>
#endif // QTUTIL_CRASH_DUMP_SUPPORTED

namespace QtUtil {

// ---------------------------------------------------------------------------
// Global configuration — change at runtime before calling installCrashHandler().
// ---------------------------------------------------------------------------

/// Directory where dump files are written. Default: "dumps"
inline QString dumpDir = "dumps";

/// Application name used in the dump filename. Default: "DumpFile"
inline QString appName = "DumpFile";

/// Application version string used in the dump filename. Default: "" (omitted)
inline QString appVersion = "";

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------
namespace dump_detail {

#if QTUTIL_CRASH_DUMP_SUPPORTED

/// Saved previous exception filter so we could chain to it (not used currently,
/// but kept for potential future use).
inline LPTOP_LEVEL_EXCEPTION_FILTER previousExceptionFilter = nullptr;

/// Saved previous handlers (for potential chaining).
inline _invalid_parameter_handler previousInvalidParamHandler = nullptr;
inline _purecall_handler          previousPureCallHandler     = nullptr;
typedef void (*SignalHandlerType)(int);
inline SignalHandlerType          previousSigAbrtHandler      = nullptr;

// -----------------------------------------------------------------------
// generateMiniDump — core dump-writing function
//
// Creates the dump directory if needed, builds the filename from
// appName / appVersion / current timestamp, then calls
// DbgHelp!MiniDumpWriteDump with MiniDumpWithDataSegs.
// Returns EXCEPTION_EXECUTE_HANDLER on success (caller should terminate),
// or EXCEPTION_CONTINUE_EXECUTION on failure (caller may continue).
// -----------------------------------------------------------------------
inline int generateMiniDump(PEXCEPTION_POINTERS pExceptionPointers)
{
    // Dynamically load DbgHelp so we don't hard-depend on a specific version.
    typedef BOOL (WINAPI *MiniDumpWriteDumpT)(
        HANDLE, DWORD, HANDLE, MINIDUMP_TYPE,
        PMINIDUMP_EXCEPTION_INFORMATION,
        PMINIDUMP_USER_STREAM_INFORMATION,
        PMINIDUMP_CALLBACK_INFORMATION);

    HMODULE hDbgHelp = LoadLibraryW(L"DbgHelp.dll");
    if (!hDbgHelp) return EXCEPTION_CONTINUE_EXECUTION;

    auto pfnMiniDumpWriteDump = reinterpret_cast<MiniDumpWriteDumpT>(
        GetProcAddress(hDbgHelp, "MiniDumpWriteDump"));
    if (!pfnMiniDumpWriteDump) {
        FreeLibrary(hDbgHelp);
        return EXCEPTION_CONTINUE_EXECUTION;
    }

    // Ensure dump directory exists.
    QDir dir;
    QString dumpDirPath = QDir(dumpDir).absolutePath();
    dir.mkpath(dumpDirPath);

    // Build filename: (appName)_(version)_yyyyMMdd_HHmmss.zzz.dmp
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss.zzz");
    QString baseName;
    if (appVersion.isEmpty())
        baseName = QString("%1_%2").arg(appName, timestamp);
    else
        baseName = QString("%1_%2_%3").arg(appName, appVersion, timestamp);
    QString filePath = dumpDirPath + "/" + baseName + ".dmp";

    qDebug() << "[QtUtil::dump] Writing minidump to:" << filePath;

    HANDLE hDumpFile = CreateFileW(
        reinterpret_cast<LPCWSTR>(filePath.utf16()),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_WRITE | FILE_SHARE_READ,
        nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (hDumpFile == INVALID_HANDLE_VALUE) {
        FreeLibrary(hDbgHelp);
        return EXCEPTION_CONTINUE_EXECUTION;
    }

    MINIDUMP_EXCEPTION_INFORMATION expParam;
    expParam.ThreadId            = GetCurrentThreadId();
    expParam.ExceptionPointers   = pExceptionPointers;
    expParam.ClientPointers      = FALSE;

    // MiniDumpWithDataSegs: includes global variables — good balance of size vs. info.
    // For full memory add MiniDumpWithFullMemory (much larger files).
    pfnMiniDumpWriteDump(
        GetCurrentProcess(), GetCurrentProcessId(),
        hDumpFile, MiniDumpWithDataSegs,
        pExceptionPointers ? &expParam : nullptr,
        nullptr, nullptr);

    CloseHandle(hDumpFile);
    FreeLibrary(hDbgHelp);
    return EXCEPTION_EXECUTE_HANDLER;
}

// -----------------------------------------------------------------------
// Exception filter for SetUnhandledExceptionFilter
// -----------------------------------------------------------------------
inline LONG WINAPI unhandledExceptionFilter(LPEXCEPTION_POINTERS lpExceptionInfo)
{
    // If a debugger is attached, let it see the exception first.
    if (IsDebuggerPresent())
        return EXCEPTION_CONTINUE_SEARCH;

    generateMiniDump(lpExceptionInfo);
    return EXCEPTION_EXECUTE_HANDLER;
}

// -----------------------------------------------------------------------
// CRT invalid-parameter handler
// -----------------------------------------------------------------------
inline void invalidParameterHandler(
    const wchar_t *expression,
    const wchar_t *function,
    const wchar_t *file,
    unsigned int   line,
    uintptr_t      /*pReserved*/)
{
    // function / file / line are only available in Debug builds.
    qDebug().nospace()
        << "[QtUtil::dump] Invalid parameter detected."
        << " Expression: " << QString::fromWCharArray(expression ? expression : L"<null>")
        << " Function: "   << QString::fromWCharArray(function   ? function   : L"<null>")
        << " File: "       << QString::fromWCharArray(file       ? file       : L"<null>")
        << " Line: "       << line;

    // Raise an exception so we can capture the call stack in the dump.
    RaiseException(EXCEPTION_NONCONTINUABLE_EXCEPTION, 0, 0, nullptr);
}

// -----------------------------------------------------------------------
// Pure virtual function call handler
// -----------------------------------------------------------------------
inline void pureCallHandler()
{
    qDebug() << "[QtUtil::dump] Pure virtual function call detected.";
    RaiseException(EXCEPTION_NONCONTINUABLE_EXCEPTION, 0, 0, nullptr);
}

// -----------------------------------------------------------------------
// Signal handler for SIGABRT (covers std::terminate → abort)
// -----------------------------------------------------------------------
inline void signalHandler(int sig)
{
    qDebug() << "[QtUtil::dump] Signal received:" << sig;
    generateMiniDump(nullptr);
    _exit(sig);
}

#endif // QTUTIL_CRASH_DUMP_SUPPORTED

} // namespace dump_detail

// ---------------------------------------------------------------------------
// installCrashHandler
//
// Registers the unhandled-exception filter, CRT handlers and SIGABRT handler
// so that a minidump is written on crash.
// On non-Windows platforms this is a no-op.
// ---------------------------------------------------------------------------
inline void installCrashHandler()
{
#if QTUTIL_CRASH_DUMP_SUPPORTED
    // 1. SEH unhandled exception filter
    dump_detail::previousExceptionFilter =
        SetUnhandledExceptionFilter(dump_detail::unhandledExceptionFilter);

    // 2. CRT invalid-parameter handler
    dump_detail::previousInvalidParamHandler =
        _set_invalid_parameter_handler(dump_detail::invalidParameterHandler);

    // 3. Pure virtual call handler
    dump_detail::previousPureCallHandler =
        _set_purecall_handler(dump_detail::pureCallHandler);

    // 4. SIGABRT → covers abort() / std::terminate
    dump_detail::previousSigAbrtHandler =
        std::signal(SIGABRT, dump_detail::signalHandler);

    qDebug() << "[QtUtil::dump] Crash handler installed. Dump directory:"
             << QDir(dumpDir).absolutePath();
#else
    qDebug() << "[QtUtil::dump] Crash dump not supported on this platform — installCrashHandler() is a no-op.";
#endif
}

// ---------------------------------------------------------------------------
// preventUnhandledExceptionFilterOverride
//
// Patches kernel32!SetUnhandledExceptionFilter so that the CRT, JIT debugger
// or third-party DLLs cannot replace the handler registered by
// installCrashHandler(). This is a common technique to ensure your crash
// handler is never silently overridden.
//
// How it works: it overwrites the first few bytes of SetUnhandledExceptionFilter
// with "xor eax,eax; ret" so any subsequent call simply returns NULL without
// doing anything.
//
// Only works on x86 and x64 Windows. No-op on other platforms.
// ---------------------------------------------------------------------------
inline void preventUnhandledExceptionFilterOverride()
{
#if QTUTIL_CRASH_DUMP_SUPPORTED
    HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
    if (!hKernel32) return;

    void *pOrgEntry = reinterpret_cast<void *>(
        GetProcAddress(hKernel32, "SetUnhandledExceptionFilter"));
    if (!pOrgEntry) return;

#ifdef _M_IX86
    // x86: xor eax,eax  |  ret 4
    unsigned char patch[] = { 0x33, 0xC0, 0xC2, 0x04, 0x00 };
#elif _M_X64
    // x64: xor eax,eax  |  ret
    unsigned char patch[] = { 0x33, 0xC0, 0xC3 };
#else
    qDebug() << "[QtUtil::dump] preventUnhandledExceptionFilterOverride: unsupported architecture.";
    return;
#endif

    DWORD dwOldProtect = 0;
    if (!VirtualProtect(pOrgEntry, sizeof(patch), PAGE_EXECUTE_READWRITE, &dwOldProtect))
        return;

    SIZE_T bytesWritten = 0;
    WriteProcessMemory(GetCurrentProcess(), pOrgEntry, patch, sizeof(patch), &bytesWritten);

    DWORD dwTemp = 0;
    VirtualProtect(pOrgEntry, sizeof(patch), dwOldProtect, &dwTemp);

    qDebug() << "[QtUtil::dump] SetUnhandledExceptionFilter patched — override blocked.";
#else
    // No-op on non-Windows
#endif
}

// ---------------------------------------------------------------------------
// generateMiniDump (public wrapper)
//
// Allows manual dump generation at any point, e.g. after catching an
// exception or on a custom signal.
// On non-Windows platforms this is a no-op and returns -1.
// ---------------------------------------------------------------------------
inline int generateMiniDump()
{
#if QTUTIL_CRASH_DUMP_SUPPORTED
    return dump_detail::generateMiniDump(nullptr);
#else
    return -1;
#endif
}

} // namespace QtUtil

#undef QTUTIL_CRASH_DUMP_SUPPORTED

#endif // DUMP_H
