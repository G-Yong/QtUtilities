/*
 * log.h — Qt logging utilities (single-file header library, C++17 inline style)
 *
 * Requires C++17 or later (for inline variables).
 * Add to your .pro:
 *   CONFIG += c++17
 *   DEFINES += QT_MESSAGELOGCONTEXT   # record file/line in all build types
 *
 * USAGE
 * -----
 * Just #include this header anywhere you need it — no macros required.
 *
 * QUICK START
 * -----------
 *   #include "log.h"
 *
 *   int main(int argc, char *argv[]) {
 *       QCoreApplication app(argc, argv);
 *
 *       QtUtil::enableANSIConsole();                        // enable ANSI colours on Windows
 *       QtUtil::logDir = "logs";                            // set log directory (default: "../log")
 *       qInstallMessageHandler(QtUtil::logToFile);          // redirect Qt messages to file + console
 *       QtUtil::redirectStdStreams();                       // capture printf + std::cout + std::cerr → log
 *       QtUtil::cleanExpiredLogFile(QtUtil::logDir, 30);   // delete log files older than 30 days
 *
 *       qInfo()       << "Qt info message";
 *       std::cout     << "C++ cout line" << std::endl;
 *       std::cerr     << "C++ cerr line" << std::endl;
 *       printf("C-style printf line %d\n", 42);
 *
 *       // Restore before exit (optional but tidy)
 *       QtUtil::restoreStdStreams();
 *   }
 *
 * HOW IT WORKS
 * -----------
 * redirectStdStreams() captures output at the OS file-descriptor level (a pipe
 * read by a background thread), so printf, std::cout/std::cerr and any
 * C-library writes are all captured by one mechanism and tee'd back to the
 * real console. On WebAssembly this function is a no-op; use Qt logging APIs
 * there because fd redirection and background threads are not generally
 * available in the browser runtime.
 */

#ifndef LOG_H
#define LOG_H

#include <QString>
#include <QDebug>
#include <QMutex>
#include <QMutexLocker>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QByteArray>
#include <QFileInfo>
#include <QObject>
#include <QStringList>
#include <iostream>
#include <string>
#include <thread>
#include <cstdio>
#include <cstdlib>

#ifdef Q_OS_WIN
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#else
#include <unistd.h>
#endif

namespace QtUtil {

// ---------------------------------------------------------------------------
// Global log directory — change at runtime before first log call.
// Default: "../log"
// ---------------------------------------------------------------------------
inline QString logDir = "../log";

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------
namespace log_detail {

// Shared mutex used by logToFile and the fd reader threads to serialise all
// writes to the log file and console.
inline QMutex logMutex;

// Saved (dup'd) fds of the real console, set while stdout/stderr are being
// redirected. logToFile and the reader threads write console output here so
// that Qt messages are never fed back into the capture pipe (which would cause
// recursion / double logging).
inline int savedStdoutFd = -1;
inline int savedStderrFd = -1;
inline bool cleanupRegistered = false;

inline bool stdStreamRedirectSupported()
{
#if defined(Q_OS_WASM) || defined(__EMSCRIPTEN__)
    return false;
#else
    return true;
#endif
}

// --- Cross-platform low-level fd helpers -----------------------------------
#ifdef Q_OS_WIN
inline int     os_pipe(int fds[2])                         { return _pipe(fds, 1 << 16, _O_BINARY); }
inline int     os_dup(int fd)                              { return _dup(fd); }
inline int     os_dup2(int oldFd, int newFd)               { return _dup2(oldFd, newFd); }
inline int     os_close(int fd)                            { return _close(fd); }
inline int     os_read(int fd, void *b, unsigned n)        { return _read(fd, b, n); }
inline int     os_write(int fd, const void *b, unsigned n) { return _write(fd, b, n); }
inline int     os_fileno(FILE *f)                          { return _fileno(f); }
#elif !defined(__EMSCRIPTEN__)
inline int     os_pipe(int fds[2])                         { return ::pipe(fds); }
inline int     os_dup(int fd)                              { return ::dup(fd); }
inline int     os_dup2(int oldFd, int newFd)               { return ::dup2(oldFd, newFd); }
inline int     os_close(int fd)                            { return ::close(fd); }
inline ssize_t os_read(int fd, void *b, size_t n)          { return ::read(fd, b, n); }
inline ssize_t os_write(int fd, const void *b, size_t n)   { return ::write(fd, b, n); }
inline int     os_fileno(FILE *f)                          { return ::fileno(f); }
#else // __EMSCRIPTEN__: pipe/dup syscalls unavailable in browsers
inline int     os_pipe(int fds[2])                         { fds[0] = fds[1] = -1; return -1; }
inline int     os_dup(int)                                 { return -1; }
inline int     os_dup2(int, int)                           { return -1; }
inline int     os_close(int fd)                            { return ::close(fd); }
inline ssize_t os_read(int fd, void *b, size_t n)          { return ::read(fd, b, n); }
inline ssize_t os_write(int fd, const void *b, size_t n)   { return ::write(fd, b, n); }
inline int     os_fileno(FILE *f)                          { return ::fileno(f); }
#endif

// Write raw bytes to a fd, handling partial writes.
inline void rawWrite(int fd, const char *data, size_t len)
{
    if (fd < 0) return;
    size_t off = 0;
    while (off < len) {
        auto n = os_write(fd, data + off, static_cast<unsigned>(len - off));
        if (n <= 0) break;
        off += static_cast<size_t>(n);
    }
}

inline void cleanupRedirectors();

inline void registerCleanup()
{
    if (cleanupRegistered) return;
    std::atexit(cleanupRedirectors);
    cleanupRegistered = true;
}

// Write a coloured formatted message to the daily log file.
// Caller must already hold logMutex.
inline void writeToFile(const QString &colorFmt, const QString &finalMsg)
{
    QDir    dir;
    QString logDirPath = QDir(logDir).absolutePath();
    if (dir.mkpath(logDirPath)) {
        QString path = logDirPath + "/"
                       + QDateTime::currentDateTime().toString("yyyyMMdd")
                       + ".log";
        QFile file(path);
        if (file.open(QIODevice::Append | QIODevice::Text)) {
            const QByteArray bytes = (colorFmt.arg(finalMsg) + "\n").toUtf8();
            file.write(bytes);
        }
    }
}

// Write a coloured formatted message straight to the real console fd,
// bypassing std::cout so Qt messages are never captured by the pipe.
// Caller must already hold logMutex.
inline void writeToConsole(const QString &colorFmt, const QString &finalMsg)
{
    int fd = (savedStdoutFd >= 0) ? savedStdoutFd : os_fileno(stdout);
    QByteArray bytes = (colorFmt.arg(finalMsg) + "\n").toLocal8Bit();
    rawWrite(fd, bytes.constData(), bytes.size());
}

} // namespace log_detail

// ---------------------------------------------------------------------------
// enableANSIConsole
// Requires Windows 10 Anniversary Update (1607) or later.
// No-op on non-Windows platforms.
// ---------------------------------------------------------------------------
inline void enableANSIConsole()
{
#ifdef Q_OS_WIN
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;
    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return;
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
#endif
}

// ---------------------------------------------------------------------------
// logToFile — Qt message handler
// Install with: qInstallMessageHandler(QtUtil::logToFile)
// Writes coloured output to the real stdout and appends to a daily log file
// under logDir named yyyyMMdd.log.
// ---------------------------------------------------------------------------
inline void logToFile(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QMutexLocker lock(&log_detail::logMutex);

    // ANSI colour codes: 31=red  32=green  33=yellow  41=red-bg  57=default
    QString colorFmt;
    QString level;
    switch (type) {
    case QtDebugMsg:    level = "DEBUG";    colorFmt = "\033[37m%1\033[0m"; break;
    case QtWarningMsg:  level = "WARNING";  colorFmt = "\033[33m%1\033[0m"; break;
    case QtCriticalMsg: level = "CRITICAL"; colorFmt = "\033[31m%1\033[0m"; break;
    case QtFatalMsg:    level = "FATAL";    colorFmt = "\033[41m%1\033[0m"; break;
    case QtInfoMsg:     level = "INFO";     colorFmt = "\033[32m%1\033[0m"; break;
    }

    QString fileStr = (context.file != nullptr) ? context.file : "UnknownFile";
    QString finalMsg = QString("[%1][%2][file:///%3:%4]\n %5")
                           .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"),
                                level, fileStr, QString::number(context.line), msg);

    log_detail::writeToFile(colorFmt, finalMsg);
    log_detail::writeToConsole(colorFmt, finalMsg);
}

// ---------------------------------------------------------------------------
// FdRedirector — captures everything written to a file descriptor (stdout or
// stderr) at the OS level: printf, std::cout/std::cerr and any C-library
// output. Each complete line is routed through the shared log system (file +
// real console) on a background reader thread.
// Not compiled on WebAssembly (pipe/dup syscalls unsupported in browsers).
// ---------------------------------------------------------------------------
#if !defined(__EMSCRIPTEN__)
class FdRedirector
{
public:
    FdRedirector(FILE *stream, const QString &level, const QString &colorFmt)
        : m_stream(stream), m_level(level), m_colorFmt(colorFmt) {}

    ~FdRedirector() { stop(); }

    FdRedirector(const FdRedirector &)            = delete;
    FdRedirector &operator=(const FdRedirector &) = delete;

    /// Begin capturing. Returns true on success.
    bool start()
    {
        if (m_active) return true;

        flushStreams();

        m_targetFd = log_detail::os_fileno(m_stream);
        if (m_targetFd < 0) return false;

        int fds[2];
        if (log_detail::os_pipe(fds) != 0) return false;
        m_pipeRead    = fds[0];
        int pipeWrite = fds[1];

        m_savedFd = log_detail::os_dup(m_targetFd);
        if (m_savedFd < 0) {
            log_detail::os_close(m_pipeRead);
            log_detail::os_close(pipeWrite);
            m_pipeRead = -1;
            return false;
        }

        // Point the target fd at the pipe's write end, then drop our extra
        // reference so the only writer left is the target fd itself. That way
        // restoring the fd later yields EOF on the read end.
        log_detail::os_dup2(pipeWrite, m_targetFd);
        log_detail::os_close(pipeWrite);

        std::setvbuf(m_stream, nullptr, _IONBF, 0);

        // Expose the saved console fd for tee + Qt-message console output.
        if (m_targetFd == log_detail::os_fileno(stdout)) log_detail::savedStdoutFd = m_savedFd;
        else                                             log_detail::savedStderrFd = m_savedFd;

        m_active = true;
        m_thread = std::thread(&FdRedirector::readerLoop, this);
        return true;
    }

    /// Stop capturing and restore the original fd. Idempotent.
    void stop()
    {
        if (!m_active) return;

        flushStreams();

        // Restoring the target fd drops the last writer reference to the pipe,
        // so the reader thread sees EOF and exits.
        log_detail::os_dup2(m_savedFd, m_targetFd);

        if (m_thread.joinable()) m_thread.join();

        if (m_targetFd == log_detail::os_fileno(stdout)) log_detail::savedStdoutFd = -1;
        else                                             log_detail::savedStderrFd = -1;

        log_detail::os_close(m_savedFd);
        log_detail::os_close(m_pipeRead);
        m_savedFd  = -1;
        m_pipeRead = -1;
        m_active   = false;
    }

private:
    void flushStreams()
    {
        std::fflush(nullptr);
        std::cout.flush();
        std::cerr.flush();
    }

    void readerLoop()
    {
        char buf[4096];
        for (;;) {
            auto n = log_detail::os_read(m_pipeRead, buf, sizeof(buf));
            if (n <= 0) break;
            m_partial.append(buf, static_cast<size_t>(n));

            size_t pos;
            while ((pos = m_partial.find('\n')) != std::string::npos) {
                processLine(m_partial.substr(0, pos));
                m_partial.erase(0, pos + 1);
            }
        }
        if (!m_partial.empty()) {
            processLine(m_partial);
            m_partial.clear();
        }
    }

    void processLine(std::string line)
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) return;

        QMutexLocker lock(&log_detail::logMutex);

        QString msg      = QString::fromLocal8Bit(line.c_str(), static_cast<int>(line.size()));
        QString finalMsg = QString("[%1][%2]\n %3")
                               .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"),
                                    m_level, msg);

        log_detail::writeToFile(m_colorFmt, finalMsg);

        // Tee the captured line back to the real console.
        QByteArray bytes = (m_colorFmt.arg(finalMsg) + "\n").toLocal8Bit();
        log_detail::rawWrite(m_savedFd, bytes.constData(), bytes.size());
    }

    FILE        *m_stream   = nullptr;
    QString      m_level;
    QString      m_colorFmt;
    int          m_targetFd = -1;
    int          m_savedFd  = -1;
    int          m_pipeRead = -1;
    bool         m_active   = false;
    std::thread  m_thread;
    std::string  m_partial;
};
#endif // !defined(__EMSCRIPTEN__)

// ---------------------------------------------------------------------------
// redirectStdout / redirectStderr — start OS-level capture of stdout / stderr
// (covers printf, std::cout/std::cerr and any C-library output).
// restoreStdout / restoreStderr  — stop capture and restore the original fd.
// All functions are idempotent. No-ops on WebAssembly.
// ---------------------------------------------------------------------------
#if !defined(__EMSCRIPTEN__)

namespace log_detail {
    inline FdRedirector *stdoutRedirector = nullptr;
    inline FdRedirector *stderrRedirector = nullptr;
} // namespace log_detail

/// Capture stdout (printf + std::cout) into the log system.
inline void redirectStdout()
{
    if (log_detail::stdoutRedirector) return;
    auto *r = new FdRedirector(stdout, "STDOUT", "\033[37m%1\033[0m");
    if (r->start()) {
        log_detail::stdoutRedirector = r;
        log_detail::registerCleanup();
    } else {
        delete r;
    }
}

/// Capture stderr (fprintf(stderr,...) + std::cerr) into the log system.
inline void redirectStderr()
{
    if (log_detail::stderrRedirector) return;
    auto *r = new FdRedirector(stderr, "STDERR", "\033[31m%1\033[0m");
    if (r->start()) {
        log_detail::stderrRedirector = r;
        log_detail::registerCleanup();
    } else {
        delete r;
    }
}

/// Stop capturing stdout and restore the original fd.
inline void restoreStdout()
{
    if (!log_detail::stdoutRedirector) return;
    log_detail::stdoutRedirector->stop();
    delete log_detail::stdoutRedirector;
    log_detail::stdoutRedirector = nullptr;
}

/// Stop capturing stderr and restore the original fd.
inline void restoreStderr()
{
    if (!log_detail::stderrRedirector) return;
    log_detail::stderrRedirector->stop();
    delete log_detail::stderrRedirector;
    log_detail::stderrRedirector = nullptr;
}

namespace log_detail {
inline void cleanupRedirectors() { restoreStdStreams(); }
} // namespace log_detail

#else // __EMSCRIPTEN__: fd redirection unavailable — empty stubs

inline void redirectStdout() {}
inline void redirectStderr() {}
inline void restoreStdout()  {}
inline void restoreStderr()  {}

namespace log_detail {
inline void cleanupRedirectors() {}
} // namespace log_detail

#endif // !defined(__EMSCRIPTEN__)

/// Convenience: capture both standard streams at once.
inline void redirectStdStreams() { redirectStdout(); redirectStderr(); }

/// Convenience: restore both standard streams at once.
inline void restoreStdStreams()  { restoreStderr();  restoreStdout(); }

// ---------------------------------------------------------------------------
// cleanExpiredLogFile
// Delete *.log files in dirPath whose names (yyyyMMdd) are older than
// keepDays days. Returns 0 on success.
// ---------------------------------------------------------------------------
inline int cleanExpiredLogFile(const QString &dirPath, int keepDays = 365)
{
    QDir dir(dirPath);
    dir.setFilter(QDir::Files | QDir::Hidden | QDir::NoSymLinks);
    dir.setNameFilters(QStringList{"*.log"});

    const QDateTime now = QDateTime::currentDateTime();

    for (const QFileInfo &fi : dir.entryInfoList()) {
        QDateTime fileDT = QDateTime::fromString(fi.baseName(), "yyyyMMdd");
        if (!fileDT.isValid()) continue;
        if (fileDT.daysTo(now) > keepDays) {
            if (QFile::remove(fi.absoluteFilePath()))
                qInfo() << QObject::tr("移除日志文件") + fi.absoluteFilePath() << keepDays;
            else
                qCritical() << QObject::tr("无法移除日志文件") + fi.absoluteFilePath();
        }
    }
    return 0;
}

} // namespace QtUtil

#endif // LOG_H
