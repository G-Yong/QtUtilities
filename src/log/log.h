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
 *       QtUtil::redirectCout();                             // capture std::cout → log
 *       QtUtil::redirectCerr();                             // capture std::cerr → log
 *       QtUtil::cleanExpiredLogFile(QtUtil::logDir, 30);   // delete log files older than 30 days
 *
 *       qInfo()       << "Qt info message";
 *       std::cout     << "C++ cout line" << std::endl;
 *       std::cerr     << "C++ cerr line" << std::endl;
 *
 *       // Restore before exit (optional but tidy)
 *       QtUtil::restoreCout();
 *       QtUtil::restoreCerr();
 *   }
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
#include <QTextStream>
#include <QTextCodec>
#include <iostream>
#include <streambuf>
#include <string>

#ifdef Q_OS_WIN
#include <windows.h>
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
namespace detail {

// Shared mutex used by both logToFile and LogStreamBuf to serialise all
// writes to the log file and console.
inline QMutex logMutex;

// Original stream buffers — saved when redirectCout/redirectCerr are called.
// logToFile always writes to originalCoutBuf (if set) to bypass any
// rdbuf substitution and avoid infinite recursion.
inline std::streambuf *originalCoutBuf = nullptr;
inline std::streambuf *originalCerrBuf = nullptr;

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
            QTextStream out(&file);
            out.setCodec(QTextCodec::codecForName("UTF-8"));
            out << colorFmt.arg(finalMsg) << "\n";
        }
    }
}

// Write a coloured formatted message directly to the real stdout,
// bypassing any rdbuf substitution to avoid recursion.
// Caller must already hold logMutex.
inline void writeToConsole(const QString &colorFmt, const QString &finalMsg)
{
    std::streambuf *buf = originalCoutBuf ? originalCoutBuf : std::cout.rdbuf();
    QByteArray bytes = (colorFmt.arg(finalMsg) + "\n").toLocal8Bit();
    buf->sputn(bytes.constData(), bytes.size());
    buf->pubsync();
}

} // namespace detail

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
    QMutexLocker lock(&detail::logMutex);

    // ANSI colour codes: 31=red  32=green  33=yellow  41=red-bg  57=default
    QString colorFmt;
    QString level;
    switch (type) {
    case QtDebugMsg:    level = "DEBUG";    colorFmt = "\033[57m%1\033[0m"; break;
    case QtWarningMsg:  level = "WARNING";  colorFmt = "\033[33m%1\033[0m"; break;
    case QtCriticalMsg: level = "CRITICAL"; colorFmt = "\033[31m%1\033[0m"; break;
    case QtFatalMsg:    level = "FATAL";    colorFmt = "\033[41m%1\033[0m"; break;
    case QtInfoMsg:     level = "INFO";     colorFmt = "\033[32m%1\033[0m"; break;
    }

    QString fileStr = (context.file != nullptr) ? context.file : "UnknownFile";
    QString finalMsg = QString("[%1][%2][file:///%3:%4]\n %5")
                           .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"),
                                level, fileStr, QString::number(context.line), msg);

    detail::writeToFile(colorFmt, finalMsg);
    detail::writeToConsole(colorFmt, finalMsg);
}

// ---------------------------------------------------------------------------
// LogStreamBuf — intercepts std::cout / std::cerr line by line and routes
// each complete line through the shared log system (file + real stdout).
// ---------------------------------------------------------------------------
class LogStreamBuf : public std::streambuf
{
public:
    LogStreamBuf(const QString &level, const QString &colorFmt, std::streambuf *originalBuf)
        : m_level(level), m_colorFmt(colorFmt), m_originalBuf(originalBuf) {}

    ~LogStreamBuf() override { if (!m_line.empty()) flushLine(); }

protected:
    int overflow(int c) override
    {
        if (c == EOF) return EOF;
        if (c == '\n') flushLine();
        else           m_line += static_cast<char>(c);
        return c;
    }

    std::streamsize xsputn(const char *s, std::streamsize n) override
    {
        for (std::streamsize i = 0; i < n; ++i)
            if (overflow(static_cast<unsigned char>(s[i])) == EOF) return i;
        return n;
    }

private:
    void flushLine()
    {
        if (m_line.empty()) return;

        QMutexLocker lock(&detail::logMutex);

        QString msg      = QString::fromLocal8Bit(m_line.c_str());
        m_line.clear();
        QString finalMsg = QString("[%1][%2]\n %3")
                               .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"),
                                    m_level, msg);

        detail::writeToFile(m_colorFmt, finalMsg);

        // Write directly to the saved original buffer — not through std::cout —
        // to avoid looping back into this streambuf.
        QByteArray bytes = (m_colorFmt.arg(finalMsg) + "\n").toLocal8Bit();
        m_originalBuf->sputn(bytes.constData(), bytes.size());
        m_originalBuf->pubsync();
    }

    QString         m_level;
    QString         m_colorFmt;
    std::streambuf *m_originalBuf;
    std::string     m_line;
};

// ---------------------------------------------------------------------------
// redirectCout / redirectCerr — install LogStreamBuf on std::cout / std::cerr.
// restoreCout / restoreCerr  — remove it and restore the original rdbuf.
// All four functions are idempotent (safe to call multiple times).
// ---------------------------------------------------------------------------
namespace detail {
    inline LogStreamBuf *coutLogBuf = nullptr;
    inline LogStreamBuf *cerrLogBuf = nullptr;
} // namespace detail

/// Redirect std::cout to the log system.
inline void redirectCout()
{
    if (detail::coutLogBuf) return;
    detail::originalCoutBuf = std::cout.rdbuf();
    detail::coutLogBuf = new LogStreamBuf("STDOUT", "\033[37m%1\033[0m",
                                          detail::originalCoutBuf);
    std::cout.rdbuf(detail::coutLogBuf);
}

/// Redirect std::cerr to the log system.
inline void redirectCerr()
{
    if (detail::cerrLogBuf) return;
    detail::originalCerrBuf = std::cerr.rdbuf();
    detail::cerrLogBuf = new LogStreamBuf("STDERR", "\033[31m%1\033[0m",
                                          detail::originalCerrBuf);
    std::cerr.rdbuf(detail::cerrLogBuf);
}

/// Restore std::cout to its original buffer.
inline void restoreCout()
{
    if (!detail::originalCoutBuf) return;
    std::cout.rdbuf(detail::originalCoutBuf);
    delete detail::coutLogBuf;
    detail::coutLogBuf      = nullptr;
    detail::originalCoutBuf = nullptr;
}

/// Restore std::cerr to its original buffer.
inline void restoreCerr()
{
    if (!detail::originalCerrBuf) return;
    std::cerr.rdbuf(detail::originalCerrBuf);
    delete detail::cerrLogBuf;
    detail::cerrLogBuf      = nullptr;
    detail::originalCerrBuf = nullptr;
}

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
