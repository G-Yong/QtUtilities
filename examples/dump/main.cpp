#include "../../src/dump.h"
#include <QCoreApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // -- Configuration (all optional, set before installCrashHandler) ---------
    QtUtil::appName    = "DumpExample";   // default: "DumpFile"
    QtUtil::appVersion = "1.0.0";         // default: "" (omitted)
    QtUtil::dumpDir    = "dumps";         // default: "dumps"

    // -- Install crash handlers -----------------------------------------------
    QtUtil::installCrashHandler();

    // -- Block CRT / JIT debugger from replacing our handler ------------------
    QtUtil::preventUnhandledExceptionFilterOverride();

    qInfo() << "Crash handler is active. Dumps will be saved under"
            << QtUtil::dumpDir;

    // -- Manual dump example --------------------------------------------------
    // At any point you can generate a dump manually:
    // QtUtil::generateMiniDump();

    // -- Trigger a crash for demonstration ------------------------------------
    // Uncomment one of the following to test:

    // 1. Null pointer access violation
    // volatile int *p = nullptr;
    // *p = 42;

    // 2. Raise an SEH exception directly
    // RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 0, nullptr);

    // 3. abort() → caught by SIGABRT handler
    // std::abort();

    qInfo() << "Exiting normally.";
    return 0;
}
