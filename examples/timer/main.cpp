#include "../../src/timer.h"

#include <QCoreApplication>
#include <QThread>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // -----------------------------------------------------------------------
    // 1. Stopwatch — manual start / restart
    // -----------------------------------------------------------------------
    QtUtil::Stopwatch sw;
    QThread::msleep(120);
    qInfo() << "[Stopwatch] elapsed:" << sw.elapsedMs() << "ms";

    // -----------------------------------------------------------------------
    // 2. ScopedTimer — logs automatically on scope exit
    // -----------------------------------------------------------------------
    {
        QtUtil::ScopedTimer t("inner work");
        QThread::msleep(50);
    }

    // -----------------------------------------------------------------------
    // 3. measureMs — time any callable in one line
    // -----------------------------------------------------------------------
    const double ms = QtUtil::measureMs([] { QThread::msleep(30); });
    qInfo() << "[measureMs] lambda:" << ms << "ms";

    // -----------------------------------------------------------------------
    // 4. formatMs — human readable durations
    // -----------------------------------------------------------------------
    qInfo() << "[formatMs] 12     ->" << QtUtil::formatMs(12);
    qInfo() << "[formatMs] 1500   ->" << QtUtil::formatMs(1500);
    qInfo() << "[formatMs] 95000  ->" << QtUtil::formatMs(95000);

    // -----------------------------------------------------------------------
    // 5. FpsCounter — smoothed frames per second
    // -----------------------------------------------------------------------
    QtUtil::FpsCounter fps;
    for (int i = 0; i < 5; ++i) {
        QThread::msleep(16);                 // ~60 FPS frame budget
        qInfo() << "[FpsCounter] frame" << i << ":"
                << QString::number(fps.tick(), 'f', 1) << "fps";
    }

    // -----------------------------------------------------------------------
    // 6. RateLimiter — gate that fires at most once per interval
    // -----------------------------------------------------------------------
    QtUtil::RateLimiter limiter(100);        // once per 100 ms
    int fired = 0;
    for (int i = 0; i < 10; ++i) {
        if (limiter.allow()) ++fired;
        QThread::msleep(30);
    }
    qInfo() << "[RateLimiter] fired" << fired << "of 10 attempts over ~300 ms";

    return 0;
}
