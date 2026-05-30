#include "../../src/log/log.h"
#include <QCoreApplication>
#include <iostream>
#include <cstdio>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QtUtil::enableANSIConsole();
    QtUtil::logDir = "logs";
    qInstallMessageHandler(QtUtil::logToFile);
    QtUtil::redirectStdStreams();                       // capture printf + std::cout + std::cerr → log
    QtUtil::cleanExpiredLogFile(QtUtil::logDir, 30);

    // Qt messages
    qInfo()     << "Application started";
    qDebug()    << "Debug message";
    qWarning()  << "Something looks odd";
    qCritical() << "Critical error occurred";

    // std::cout / std::cerr — captured at the fd level
    std::cout << "Hello from std::cout" << std::endl;
    std::cerr << "Hello from std::cerr" << std::endl;

    // printf / fprintf — now captured by the same mechanism
    printf("Hello from printf %d\n", 42);
    fprintf(stderr, "Hello from fprintf(stderr) %s\n", "oops");
    fflush(stdout);
    fflush(stderr);

    QtUtil::restoreStdStreams();

    return 0;
}
