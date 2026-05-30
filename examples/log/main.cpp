#include "../../src/log/log.h"
#include <QCoreApplication>
#include <iostream>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QtUtil::enableANSIConsole();
    QtUtil::logDir = "logs";
    qInstallMessageHandler(QtUtil::logToFile);
    QtUtil::redirectCout();                             // capture std::cout → log
    QtUtil::redirectCerr();                             // capture std::cerr → log
    QtUtil::cleanExpiredLogFile(QtUtil::logDir, 30);

    // Qt messages
    qInfo()     << "Application started";
    qDebug()    << "Debug message";
    qWarning()  << "Something looks odd";
    qCritical() << "Critical error occurred";

    // std::cout / std::cerr — also captured and written to log file
    std::cout << "Hello from std::cout" << std::endl;
    std::cerr << "Hello from std::cerr" << std::endl;

    QtUtil::restoreCout();
    QtUtil::restoreCerr();

    return 0;
}
