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
 *       QtUtil::cleanExpiredLogFile(QtUtil::logDir, 30);   // delete log files older than 30 days
 *
 *       qInfo()    << "Application started";
 *       qDebug()   << "Debug message";
 *       qWarning() << "Warning message";
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
// Enable ANSI escape-code colour output in the Windows console.
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
// Qt message handler — install with qInstallMessageHandler(logToFile).
// Writes coloured output to stdout and appends to a daily log file under
// __logDir named yyyyMMdd.log.
// ---------------------------------------------------------------------------
inline void logToFile(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    static QMutex mutex;
    QMutexLocker lock(&mutex);

    // ANSI colour codes: 31=red 32=green 33=yellow 41=red-bg 57=default
    QString colorFmt;
    QString level;

    switch (type) {
    case QtDebugMsg:
        level    = "DEBUG";
        colorFmt = "\033[57m%1\033[0m";
        break;
    case QtWarningMsg:
        level    = "WARNING";
        colorFmt = "\033[33m%1\033[0m";
        break;
    case QtCriticalMsg:
        level    = "CRITICAL";
        colorFmt = "\033[31m%1\033[0m";
        break;
    case QtFatalMsg:
        level    = "FATAL";
        colorFmt = "\033[41m%1\033[0m";
        break;
    case QtInfoMsg:
        level    = "INFO";
        colorFmt = "\033[32m%1\033[0m";
        break;
    }

    QString fileStr = (context.file != nullptr) ? context.file : "UnknownFile";
    int     lineNum =  context.line;

    QString finalMsg = QString("[%1][%2][file:///%3:%4]\n %5")
                           .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"),
                                level,
                                fileStr,
                                QString::number(lineNum),
                                msg);

    // --- write to daily log file ---
    {
        QDir    dir;
        QString logDirPath = QDir(logDir).absolutePath();
        if (dir.mkpath(logDirPath)) {
            QString logFileName = logDirPath + "/"
                                  + QDateTime::currentDateTime().toString("yyyyMMdd")
                                  + ".log";
            QFile file(logFileName);
            if (file.open(QIODevice::Append | QIODevice::Text)) {
                QTextStream out(&file);
                out.setCodec(QTextCodec::codecForName("UTF-8"));
                out << colorFmt.arg(finalMsg) << "\n";
                file.close();
            }
        }
    }

    // --- write to stdout ---
    std::cout << colorFmt.arg(finalMsg).toLocal8Bit().data() << std::endl;
}

// ---------------------------------------------------------------------------
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
            if (QFile::remove(fi.absoluteFilePath())) {
                qInfo() << QObject::tr("移除日志文件") + fi.absoluteFilePath() << keepDays;
            } else {
                qCritical() << QObject::tr("无法移除日志文件") + fi.absoluteFilePath();
            }
        }
    }

    return 0;
}

} // namespace QtUtil

#endif // LOG_H
