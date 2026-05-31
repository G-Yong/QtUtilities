#include "../../src/system.h"

#include <QCoreApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // -----------------------------------------------------------------------
    // 1. Identity — UUIDs, host and OS information
    // -----------------------------------------------------------------------
    qInfo() << "[UUID]        random :" << QtUtil::generateUuid();
    qInfo() << "[UUID]        machine:" << QtUtil::machineUuid();
    qInfo() << "[Host]        name   :" << QtUtil::hostName();
    qInfo() << "[OS]          name   :" << QtUtil::osName();
    qInfo() << "[OS]          kernel :" << QtUtil::kernelVersion();
    qInfo() << "[OS]          arch   :" << QtUtil::cpuArchitecture();

    // -----------------------------------------------------------------------
    // 2. CPU — model, core count and usage
    // -----------------------------------------------------------------------
    qInfo() << "[CPU]         model  :" << QtUtil::cpuModelName();
    qInfo() << "[CPU]         cores  :" << QtUtil::cpuCoreCount();
    qInfo() << "[CPU]         usage  :" << QtUtil::cpuUsage()        << "%";
    qInfo() << "[CPU]         process:" << QtUtil::processCpuUsage() << "%";

    // -----------------------------------------------------------------------
    // 3. Memory — whole machine and this process
    // -----------------------------------------------------------------------
    const QtUtil::MemoryStatus mem = QtUtil::memoryStatus();
    qInfo() << "[Memory]      total  :" << QtUtil::formatBytes(mem.totalPhys);
    qInfo() << "[Memory]      used   :" << QtUtil::formatBytes(mem.usedPhys)
            << QString("(%1%)").arg(mem.usedPercent, 0, 'f', 1);
    qInfo() << "[Memory]      avail  :" << QtUtil::formatBytes(mem.availPhys);
    qInfo() << "[Memory]      swap   :" << QtUtil::formatBytes(mem.availSwap)
            << "/" << QtUtil::formatBytes(mem.totalSwap);
    qInfo() << "[Memory]      process:" << QtUtil::formatBytes(QtUtil::processMemoryUsage());

    // -----------------------------------------------------------------------
    // 4. Disk — capacity of the system drive
    // -----------------------------------------------------------------------
#ifdef Q_OS_WIN
    const QtUtil::DiskStatus disk = QtUtil::diskStatus("C:/");
#else
    const QtUtil::DiskStatus disk = QtUtil::diskStatus("/");
#endif
    qInfo() << "[Disk]        total  :" << QtUtil::formatBytes(disk.total);
    qInfo() << "[Disk]        used   :" << QtUtil::formatBytes(disk.used)
            << QString("(%1%)").arg(disk.usedPercent, 0, 'f', 1);
    qInfo() << "[Disk]        avail  :" << QtUtil::formatBytes(disk.available);

    // -----------------------------------------------------------------------
    // 5. Uptime
    // -----------------------------------------------------------------------
    const qint64 uptime = QtUtil::systemUptimeSeconds();
    qInfo() << "[Uptime]      seconds:" << uptime
            << "(" << QtUtil::formatDuration(uptime) << ")";

    // -----------------------------------------------------------------------
    // 6. Continuous monitoring — stateful, non-blocking samplers
    // -----------------------------------------------------------------------
    QtUtil::CpuUsageMonitor        cpuMon;
    QtUtil::ProcessCpuUsageMonitor procMon;
    for (int i = 0; i < 3; ++i) {
        QThread::msleep(500);
        qInfo() << "[Monitor]     cpu/proc:"
                << QString::number(cpuMon.poll(),  'f', 1) << "% /"
                << QString::number(procMon.poll(), 'f', 1) << "%";
    }

    return 0;
}
