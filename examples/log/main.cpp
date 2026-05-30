#include "../../src/log/log.h"
#include <QCoreApplication>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QtUtil::enableANSIConsole();
    QtUtil::logDir = "logs";
    qInstallMessageHandler(QtUtil::logToFile);
    QtUtil::cleanExpiredLogFile(QtUtil::logDir, 30);

    qInfo()     << "Application started";
    qDebug()    << "Debug message";
    qWarning()  << "Something looks odd";
    qCritical() << "Critical error occurred";

    return 0;
}
