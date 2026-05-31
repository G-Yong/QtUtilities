/*
 * system.h — System / hardware information utilities
 *            (single-file header library, C++17 inline style)
 *
 * Requires C++17 or later (for inline variables/functions).
 * Requires only Qt Core — no extra Qt modules.
 *   QT     += core
 *   CONFIG += c++17
 * On Windows the psapi library is linked automatically via #pragma comment
 * when building with MSVC. With MinGW add:  LIBS += -lpsapi
 *
 * USAGE
 * -----
 * Just #include this header anywhere you need it — no .cpp compilation required.
 * Every function lives in the QtUtil namespace and is implemented for both
 * Windows and Linux (POSIX). On unsupported platforms a sensible empty/zero
 * value is returned instead of failing to compile.
 *
 * QUICK START
 * -----------
 *   #include "system.h"
 *
 *   // Identity
 *   QString uuid    = QtUtil::generateUuid();   // random per-call UUID
 *   QString machine = QtUtil::machineUuid();    // stable per-machine UUID
 *   QString host    = QtUtil::hostName();
 *
 *   // CPU
 *   qInfo() << QtUtil::cpuModelName() << QtUtil::cpuCoreCount() << "cores";
 *   qInfo() << "CPU usage:" << QtUtil::cpuUsage() << "%";       // whole machine
 *   qInfo() << "Proc CPU :" << QtUtil::processCpuUsage() << "%"; // this process
 *
 *   // Memory
 *   QtUtil::MemoryStatus m = QtUtil::memoryStatus();
 *   qInfo() << "RAM used:" << QtUtil::formatBytes(m.usedPhys)
 *           << "/"         << QtUtil::formatBytes(m.totalPhys)
 *           << "("         << m.usedPercent << "% )";
 *   qInfo() << "Process RSS:" << QtUtil::formatBytes(QtUtil::processMemoryUsage());
 *
 *   // Disk
 *   QtUtil::DiskStatus d = QtUtil::diskStatus("C:/");           // or "/" on Linux
 *   qInfo() << "Disk free:" << QtUtil::formatBytes(d.available);
 *
 *   // Continuous monitoring (non-blocking, poll on a timer)
 *   QtUtil::CpuUsageMonitor cpu;     // create once
 *   double pct = cpu.poll();         // % since previous poll() call
 *
 * FUNCTIONS
 * ---------
 *   QString       generateUuid       (bool withBraces = false)
 *   QString       machineUuid        ()
 *   QString       hostName           ()
 *   QString       osName             ()
 *   QString       kernelVersion      ()
 *   QString       cpuArchitecture    ()
 *   QString       cpuModelName       ()
 *   int           cpuCoreCount       ()
 *   double        cpuUsage           (int sampleMs = 200)
 *   double        processCpuUsage    (int sampleMs = 200)
 *   MemoryStatus  memoryStatus       ()
 *   quint64       processMemoryUsage ()
 *   DiskStatus    diskStatus         (const QString &path)
 *   qint64        systemUptimeSeconds()
 *   QString       formatBytes        (quint64 bytes, int precision = 2)
 *   QString       formatDuration     (qint64 seconds)
 *
 * CLASSES
 * -------
 *   CpuUsageMonitor      — stateful whole-machine CPU % sampler (poll())
 *   ProcessCpuUsageMonitor — stateful current-process CPU % sampler (poll())
 */

#ifndef QTUTIL_SYSTEM_H
#define QTUTIL_SYSTEM_H

// Ensure a Windows 7+ API level *before* any header (including Qt's) pulls in
// <windows.h>, so GetTickCount64 / GetSystemTimes are declared.
#if defined(_WIN32)
#  ifndef _WIN32_WINNT
#    define _WIN32_WINNT 0x0601
#  endif
#  ifndef WINVER
#    define WINVER 0x0601
#  endif
#endif

#include <QString>
#include <QSysInfo>
#include <QUuid>
#include <QFile>
#include <QByteArray>
#include <QThread>

#include <thread>
#include <chrono>

#ifdef Q_OS_WIN
#  ifndef _WIN32_WINNT
#    define _WIN32_WINNT 0x0601   // Windows 7+: GetTickCount64, GetSystemTimes
#  endif
#  ifndef WINVER
#    define WINVER 0x0601
#  endif
#  include <windows.h>
#  include <psapi.h>
#  if defined(_MSC_VER)
#    pragma comment(lib, "psapi.lib")
#  endif
#else
#  include <unistd.h>
#  include <sys/types.h>
#  include <sys/statvfs.h>
#  include <sys/sysinfo.h>
#endif

namespace QtUtil {

// ===========================================================================
// Data structures
// ===========================================================================

// Snapshot of physical/virtual memory in bytes.
struct MemoryStatus {
    quint64 totalPhys   = 0;   // total installed physical RAM
    quint64 availPhys   = 0;   // currently available physical RAM
    quint64 usedPhys    = 0;   // totalPhys - availPhys
    double  usedPercent = 0.0; // 0..100
    quint64 totalSwap   = 0;   // total page file / swap
    quint64 availSwap   = 0;   // available page file / swap
};

// Snapshot of a single filesystem / mount point in bytes.
struct DiskStatus {
    quint64 total       = 0;   // total capacity
    quint64 free        = 0;   // total free space
    quint64 available   = 0;   // free space available to the current user
    quint64 used        = 0;   // total - free
    double  usedPercent = 0.0; // 0..100
};

// ===========================================================================
// Internal helpers
// ===========================================================================
namespace system_detail {

#ifdef Q_OS_WIN
// Convert a Win32 FILETIME to a 64-bit unsigned tick count.
inline quint64 fileTimeToUInt64(const FILETIME &ft)
{
    ULARGE_INTEGER v;
    v.LowPart  = ft.dwLowDateTime;
    v.HighPart = ft.dwHighDateTime;
    return static_cast<quint64>(v.QuadPart);
}

// Read a string value from a 64-bit view of the registry (avoids WOW64
// redirection so a 32-bit build still sees the real machine values).
inline QString readRegistryString(HKEY root, const wchar_t *subKey, const wchar_t *valueName)
{
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(root, subKey, 0, KEY_READ | KEY_WOW64_64KEY, &hKey) != ERROR_SUCCESS)
        return QString();

    wchar_t buffer[512];
    DWORD   size = sizeof(buffer);
    DWORD   type = 0;
    QString result;
    if (RegQueryValueExW(hKey, valueName, nullptr, &type,
                         reinterpret_cast<LPBYTE>(buffer), &size) == ERROR_SUCCESS
        && (type == REG_SZ || type == REG_EXPAND_SZ)) {
        result = QString::fromWCharArray(buffer, static_cast<int>(size / sizeof(wchar_t)));
        // Trim a trailing NUL if present.
        while (!result.isEmpty() && result.endsWith(QChar(QChar::Null)))
            result.chop(1);
        result = result.trimmed();
    }
    RegCloseKey(hKey);
    return result;
}
#else
// Read the whole content of a /proc or /sys file as a trimmed string.
inline QString readProcFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    return QString::fromUtf8(f.readAll()).trimmed();
}

// Parse the aggregate CPU line of /proc/stat into (total ticks, idle ticks).
inline bool readLinuxCpuTimes(quint64 &total, quint64 &idle)
{
    QFile f("/proc/stat");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    const QByteArray line = f.readLine(); // first line: "cpu  u n s i io irq ..."
    const QList<QByteArray> parts = line.simplified().split(' ');
    if (parts.isEmpty() || parts.first() != "cpu")
        return false;

    quint64 sum  = 0;
    quint64 idl  = 0;
    for (int i = 1; i < parts.size(); ++i) {
        const quint64 v = parts.at(i).toULongLong();
        sum += v;
        if (i == 4 || i == 5)   // idle + iowait
            idl += v;
    }
    total = sum;
    idle  = idl;
    return true;
}

// Read the current process busy ticks (utime + stime) from /proc/self/stat.
inline quint64 readLinuxProcessTicks()
{
    QFile f("/proc/self/stat");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return 0;

    const QByteArray content = f.readAll();
    // Field 14 = utime, 15 = stime (1-based). The process name (field 2) may
    // contain spaces/parentheses, so split after the closing ')'.
    const int rparen = content.lastIndexOf(')');
    if (rparen < 0)
        return 0;

    const QList<QByteArray> parts =
        content.mid(rparen + 2).simplified().split(' ');
    // After the ')' the next token is field 3 (state). utime is field 14,
    // which is index (14 - 3) = 11 in this list; stime is index 12.
    if (parts.size() <= 12)
        return 0;
    return parts.at(11).toULongLong() + parts.at(12).toULongLong();
}
#endif

// Sleep helper used by the convenience sampling functions.
inline void sleepMs(int ms)
{
    if (ms > 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

} // namespace system_detail

// ===========================================================================
// Identity
// ===========================================================================

// Generate a fresh random (version 4) UUID. With withBraces == true the form
// is "{xxxxxxxx-....}", otherwise the bare "xxxxxxxx-...." form is returned.
inline QString generateUuid(bool withBraces = false)
{
    const QString s = QUuid::createUuid().toString();
    return withBraces ? s : s.mid(1, s.size() - 2);
}

// Return a stable identifier that is unique to the current machine.
// Windows : HKLM\SOFTWARE\Microsoft\Cryptography\MachineGuid
// Linux   : /etc/machine-id (falls back to /var/lib/dbus/machine-id)
// Falls back to Qt's QSysInfo::machineUniqueId() when the above are missing.
inline QString machineUuid()
{
#ifdef Q_OS_WIN
    const QString guid = system_detail::readRegistryString(
        HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Cryptography",
        L"MachineGuid");
    if (!guid.isEmpty())
        return guid;
#else
    QString id = system_detail::readProcFile("/etc/machine-id");
    if (id.isEmpty())
        id = system_detail::readProcFile("/var/lib/dbus/machine-id");
    if (!id.isEmpty())
        return id;
#endif
    return QString::fromLatin1(QSysInfo::machineUniqueId());
}

// Local host (computer) name.
inline QString hostName()
{
    return QSysInfo::machineHostName();
}

// Human-readable operating system name, e.g. "Windows 10 Version 2009".
inline QString osName()
{
    return QSysInfo::prettyProductName();
}

// Kernel version string, e.g. "10.0.19045" or "5.15.0-91-generic".
inline QString kernelVersion()
{
    return QSysInfo::kernelVersion();
}

// CPU architecture the application was built for, e.g. "x86_64".
inline QString cpuArchitecture()
{
    return QSysInfo::currentCpuArchitecture();
}

// ===========================================================================
// CPU
// ===========================================================================

// Number of logical processors available to the application.
inline int cpuCoreCount()
{
    const int n = QThread::idealThreadCount();
    return n > 0 ? n : 1;
}

// Marketing name of the processor, e.g. "Intel(R) Core(TM) i7-9700 CPU".
inline QString cpuModelName()
{
#ifdef Q_OS_WIN
    return system_detail::readRegistryString(
        HKEY_LOCAL_MACHINE,
        L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
        L"ProcessorNameString");
#else
    QFile f("/proc/cpuinfo");
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        while (!f.atEnd()) {
            const QByteArray line = f.readLine();
            if (line.startsWith("model name")) {
                const int colon = line.indexOf(':');
                if (colon >= 0)
                    return QString::fromUtf8(line.mid(colon + 1)).trimmed();
            }
        }
    }
    return QString();
#endif
}

// ---------------------------------------------------------------------------
// CpuUsageMonitor
// Stateful whole-machine CPU utilisation sampler. Construct once, then call
// poll() on a timer; each call returns the average CPU usage (0..100 %) over
// the interval since the previous poll(). The first poll() after construction
// measures against the construction time.
// ---------------------------------------------------------------------------
class CpuUsageMonitor
{
public:
    CpuUsageMonitor() { reset(); }

    // Re-seed the baseline sample to "now".
    void reset()
    {
#ifdef Q_OS_WIN
        FILETIME idle, kernel, user;
        if (GetSystemTimes(&idle, &kernel, &user)) {
            m_idle   = system_detail::fileTimeToUInt64(idle);
            m_kernel = system_detail::fileTimeToUInt64(kernel);
            m_user   = system_detail::fileTimeToUInt64(user);
        }
#else
        system_detail::readLinuxCpuTimes(m_total, m_idle);
#endif
    }

    // Return CPU usage (0..100 %) since the previous poll()/reset().
    double poll()
    {
#ifdef Q_OS_WIN
        FILETIME idle, kernel, user;
        if (!GetSystemTimes(&idle, &kernel, &user))
            return 0.0;

        const quint64 i = system_detail::fileTimeToUInt64(idle);
        const quint64 k = system_detail::fileTimeToUInt64(kernel);
        const quint64 u = system_detail::fileTimeToUInt64(user);

        // kernel already includes idle time; total busy = (kernel+user) - idle.
        const quint64 totalDiff = (k - m_kernel) + (u - m_user);
        const quint64 idleDiff  = (i - m_idle);

        m_idle = i; m_kernel = k; m_user = u;

        if (totalDiff == 0)
            return 0.0;
        const double busy = static_cast<double>(totalDiff - idleDiff);
        return clampPercent(100.0 * busy / static_cast<double>(totalDiff));
#else
        quint64 total = 0, idle = 0;
        if (!system_detail::readLinuxCpuTimes(total, idle))
            return 0.0;

        const quint64 totalDiff = total - m_total;
        const quint64 idleDiff  = idle  - m_idle;

        m_total = total; m_idle = idle;

        if (totalDiff == 0)
            return 0.0;
        const double busy = static_cast<double>(totalDiff - idleDiff);
        return clampPercent(100.0 * busy / static_cast<double>(totalDiff));
#endif
    }

private:
    static double clampPercent(double v)
    {
        if (v < 0.0)   return 0.0;
        if (v > 100.0) return 100.0;
        return v;
    }

#ifdef Q_OS_WIN
    quint64 m_idle = 0, m_kernel = 0, m_user = 0;
#else
    quint64 m_total = 0, m_idle = 0;
#endif
};

// ---------------------------------------------------------------------------
// ProcessCpuUsageMonitor
// Stateful current-process CPU utilisation sampler. poll() returns the process
// CPU usage (0..100 %, where 100 % means one full logical core) averaged over
// the interval since the previous poll().
// ---------------------------------------------------------------------------
class ProcessCpuUsageMonitor
{
public:
    ProcessCpuUsageMonitor() { reset(); }

    void reset()
    {
        m_wall = std::chrono::steady_clock::now();
#ifdef Q_OS_WIN
        FILETIME c, e, k, u;
        if (GetProcessTimes(GetCurrentProcess(), &c, &e, &k, &u)) {
            m_kernel = system_detail::fileTimeToUInt64(k);
            m_user   = system_detail::fileTimeToUInt64(u);
        }
#else
        m_ticks = system_detail::readLinuxProcessTicks();
#endif
    }

    double poll()
    {
        const auto now = std::chrono::steady_clock::now();
        const double wallSec =
            std::chrono::duration<double>(now - m_wall).count();
        m_wall = now;
        if (wallSec <= 0.0)
            return 0.0;

        const int cores = cpuCoreCount();
#ifdef Q_OS_WIN
        FILETIME c, e, k, u;
        if (!GetProcessTimes(GetCurrentProcess(), &c, &e, &k, &u))
            return 0.0;
        const quint64 kk = system_detail::fileTimeToUInt64(k);
        const quint64 uu = system_detail::fileTimeToUInt64(u);
        // FILETIME deltas are in 100-ns units.
        const double busySec =
            static_cast<double>((kk - m_kernel) + (uu - m_user)) * 1e-7;
        m_kernel = kk; m_user = uu;
#else
        const quint64 ticks = system_detail::readLinuxProcessTicks();
        const long hz = sysconf(_SC_CLK_TCK);
        const double busySec = (hz > 0)
            ? static_cast<double>(ticks - m_ticks) / static_cast<double>(hz)
            : 0.0;
        m_ticks = ticks;
#endif
        double pct = 100.0 * busySec / (wallSec * static_cast<double>(cores));
        if (pct < 0.0)   pct = 0.0;
        if (pct > 100.0) pct = 100.0;
        return pct;
    }

private:
    std::chrono::steady_clock::time_point m_wall;
#ifdef Q_OS_WIN
    quint64 m_kernel = 0, m_user = 0;
#else
    quint64 m_ticks = 0;
#endif
};

// Convenience: whole-machine CPU usage (0..100 %) sampled over sampleMs
// milliseconds. This call blocks for sampleMs — prefer CpuUsageMonitor for
// continuous, non-blocking monitoring.
inline double cpuUsage(int sampleMs = 200)
{
    CpuUsageMonitor m;
    system_detail::sleepMs(sampleMs);
    return m.poll();
}

// Convenience: current-process CPU usage (0..100 %) sampled over sampleMs
// milliseconds. Blocks for sampleMs — prefer ProcessCpuUsageMonitor for
// continuous monitoring.
inline double processCpuUsage(int sampleMs = 200)
{
    ProcessCpuUsageMonitor m;
    system_detail::sleepMs(sampleMs);
    return m.poll();
}

// ===========================================================================
// Memory
// ===========================================================================

// Current physical/virtual memory usage of the whole machine.
inline MemoryStatus memoryStatus()
{
    MemoryStatus s;
#ifdef Q_OS_WIN
    MEMORYSTATUSEX m;
    m.dwLength = sizeof(m);
    if (GlobalMemoryStatusEx(&m)) {
        s.totalPhys = m.ullTotalPhys;
        s.availPhys = m.ullAvailPhys;
        s.usedPhys  = m.ullTotalPhys - m.ullAvailPhys;
        s.totalSwap = m.ullTotalPageFile;
        s.availSwap = m.ullAvailPageFile;
    }
#else
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        const quint64 unit = info.mem_unit ? info.mem_unit : 1;
        s.totalPhys = static_cast<quint64>(info.totalram)  * unit;
        s.availPhys = static_cast<quint64>(info.freeram + info.bufferram) * unit;
        if (s.availPhys > s.totalPhys) s.availPhys = s.totalPhys;
        s.usedPhys  = s.totalPhys - s.availPhys;
        s.totalSwap = static_cast<quint64>(info.totalswap) * unit;
        s.availSwap = static_cast<quint64>(info.freeswap)  * unit;
    }
#endif
    if (s.totalPhys > 0)
        s.usedPercent = 100.0 * static_cast<double>(s.usedPhys)
                              / static_cast<double>(s.totalPhys);
    return s;
}

// Physical memory currently used by the calling process (working set / RSS),
// in bytes.
inline quint64 processMemoryUsage()
{
#ifdef Q_OS_WIN
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        return static_cast<quint64>(pmc.WorkingSetSize);
    return 0;
#else
    // VmRSS is reported in kB in /proc/self/status.
    QFile f("/proc/self/status");
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        while (!f.atEnd()) {
            const QByteArray line = f.readLine();
            if (line.startsWith("VmRSS:")) {
                const QList<QByteArray> p = line.simplified().split(' ');
                if (p.size() >= 2)
                    return p.at(1).toULongLong() * 1024ULL;
            }
        }
    }
    return 0;
#endif
}

// ===========================================================================
// Disk
// ===========================================================================

// Capacity / free space of the filesystem that contains path. On Windows pass
// a drive root like "C:/"; on Linux pass any path on the mount, e.g. "/".
inline DiskStatus diskStatus(const QString &path)
{
    DiskStatus s;
#ifdef Q_OS_WIN
    ULARGE_INTEGER freeToCaller, total, totalFree;
    const std::wstring wpath = path.toStdWString();
    if (GetDiskFreeSpaceExW(wpath.c_str(), &freeToCaller, &total, &totalFree)) {
        s.total     = static_cast<quint64>(total.QuadPart);
        s.free      = static_cast<quint64>(totalFree.QuadPart);
        s.available = static_cast<quint64>(freeToCaller.QuadPart);
        s.used      = s.total - s.free;
    }
#else
    struct statvfs vfs;
    if (statvfs(path.toUtf8().constData(), &vfs) == 0) {
        const quint64 bsize = vfs.f_frsize ? vfs.f_frsize : vfs.f_bsize;
        s.total     = static_cast<quint64>(vfs.f_blocks) * bsize;
        s.free      = static_cast<quint64>(vfs.f_bfree)  * bsize;
        s.available = static_cast<quint64>(vfs.f_bavail) * bsize;
        s.used      = s.total - s.free;
    }
#endif
    if (s.total > 0)
        s.usedPercent = 100.0 * static_cast<double>(s.used)
                              / static_cast<double>(s.total);
    return s;
}

// ===========================================================================
// Uptime
// ===========================================================================

// Seconds elapsed since the system was last booted.
inline qint64 systemUptimeSeconds()
{
#ifdef Q_OS_WIN
    return static_cast<qint64>(GetTickCount64() / 1000ULL);
#else
    struct sysinfo info;
    if (sysinfo(&info) == 0)
        return static_cast<qint64>(info.uptime);
    return 0;
#endif
}

// ===========================================================================
// Formatting helpers
// ===========================================================================

// Format a byte count as a human-readable string, e.g. 1536 -> "1.50 KB".
inline QString formatBytes(quint64 bytes, int precision = 2)
{
    static const char *units[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB"};
    double value = static_cast<double>(bytes);
    int    unit  = 0;
    while (value >= 1024.0 && unit < 6) {
        value /= 1024.0;
        ++unit;
    }
    return QString::number(value, 'f', unit == 0 ? 0 : precision)
           + ' ' + units[unit];
}

// Format a duration in seconds as "Dd HH:MM:SS" (days omitted when zero).
inline QString formatDuration(qint64 seconds)
{
    if (seconds < 0)
        seconds = 0;
    const qint64 days = seconds / 86400;
    const qint64 hrs  = (seconds % 86400) / 3600;
    const qint64 mins = (seconds % 3600) / 60;
    const qint64 secs = seconds % 60;

    QString s;
    if (days > 0)
        s += QString::number(days) + "d ";
    s += QString("%1:%2:%3")
             .arg(hrs,  2, 10, QChar('0'))
             .arg(mins, 2, 10, QChar('0'))
             .arg(secs, 2, 10, QChar('0'));
    return s;
}

} // namespace QtUtil

#endif // QTUTIL_SYSTEM_H
