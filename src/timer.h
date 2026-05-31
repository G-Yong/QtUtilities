/*
 * timer.h — Timing & profiling utilities (single-file header library, C++17 inline style)
 *
 * Requires C++17 or later (for inline variables/functions).
 * Requires only Qt Core — no extra Qt modules.
 *   QT     += core
 *   CONFIG += c++17
 *
 * USAGE
 * -----
 * Just #include this header anywhere you need it — no .cpp compilation required.
 * Everything lives in the QtUtil namespace.
 *
 * QUICK START
 * -----------
 *   #include "timer.h"
 *
 *   // Manual stopwatch
 *   QtUtil::Stopwatch sw;
 *   heavyWork();
 *   qInfo() << "took" << sw.elapsedMs() << "ms";
 *   sw.restart();
 *
 *   // Scoped timer — logs automatically when it goes out of scope
 *   {
 *       QtUtil::ScopedTimer t("load config");
 *       loadConfig();
 *   } // prints: [ScopedTimer] load config: 12.345 ms
 *
 *   // One-liner to time any callable
 *   double ms = QtUtil::measureMs([]{ heavyWork(); });
 *
 *   // Frame-rate counter
 *   QtUtil::FpsCounter fps;
 *   while (running) { render(); double f = fps.tick(); }
 *
 *   // Rate limiter / throttle
 *   QtUtil::RateLimiter limiter(1000);   // allow at most once per 1000 ms
 *   if (limiter.allow()) doExpensiveThing();
 *
 *   // Human-readable durations
 *   QString s = QtUtil::formatMs(95000);   // "1 m 35 s"
 *
 * CLASSES
 * -------
 *   Stopwatch     — start/restart + elapsed in us/ms/s
 *   ScopedTimer   — RAII timer that logs its lifetime via qDebug on destruction
 *   FpsCounter    — smoothed frames-per-second counter (call tick() per frame)
 *   RateLimiter   — returns true at most once per interval (token gate)
 *
 * FUNCTIONS
 * ---------
 *   template<class F> double measureMs(F &&fn)   // run fn once, return elapsed ms
 *   QString formatMs (qint64 ms)
 *   QString formatUs (qint64 us)
 */

#ifndef QTUTIL_TIMER_H
#define QTUTIL_TIMER_H

#include <QString>
#include <QDebug>

#include <chrono>
#include <utility>

namespace QtUtil {

// ---------------------------------------------------------------------------
// Stopwatch
// A monotonic high-resolution stopwatch. Construction starts it; restart()
// re-seeds the origin to "now". Elapsed time is available in several units.
// ---------------------------------------------------------------------------
class Stopwatch
{
public:
    using clock = std::chrono::steady_clock;

    Stopwatch() : m_start(clock::now()) {}

    // Re-seed the origin to now.
    void restart() { m_start = clock::now(); }

    qint64 elapsedNs() const
    {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
                   clock::now() - m_start).count();
    }
    qint64 elapsedUs() const { return elapsedNs() / 1000; }
    qint64 elapsedMs() const { return elapsedNs() / 1000000; }
    double elapsedSec() const
    {
        return std::chrono::duration<double>(clock::now() - m_start).count();
    }

private:
    clock::time_point m_start;
};

// ---------------------------------------------------------------------------
// formatMs / formatUs — turn a duration into a compact human string.
//   formatMs(95000) -> "1 m 35 s"
//   formatMs(1500)  -> "1.500 s"
//   formatMs(12)    -> "12 ms"
// ---------------------------------------------------------------------------
inline QString formatMs(qint64 ms)
{
    if (ms < 0) ms = 0;
    if (ms < 1000)
        return QString::number(ms) + " ms";
    if (ms < 60000)
        return QString::number(ms / 1000.0, 'f', 3) + " s";

    const qint64 totalSec = ms / 1000;
    const qint64 h = totalSec / 3600;
    const qint64 m = (totalSec % 3600) / 60;
    const qint64 s = totalSec % 60;
    QString out;
    if (h > 0) out += QString::number(h) + " h ";
    if (h > 0 || m > 0) out += QString::number(m) + " m ";
    out += QString::number(s) + " s";
    return out;
}

inline QString formatUs(qint64 us)
{
    if (us < 0) us = 0;
    if (us < 1000)
        return QString::number(us) + " us";
    return formatMs(us / 1000);
}

// ---------------------------------------------------------------------------
// ScopedTimer
// RAII timer: measures the time between construction and destruction and logs
// it through qDebug() with an optional label. Handy for ad-hoc profiling of a
// scope without manual start/stop bookkeeping.
// ---------------------------------------------------------------------------
class ScopedTimer
{
public:
    explicit ScopedTimer(QString label = QString())
        : m_label(std::move(label)) {}

    // Non-copyable, non-movable (it owns a fixed lifetime measurement).
    ScopedTimer(const ScopedTimer &)            = delete;
    ScopedTimer &operator=(const ScopedTimer &) = delete;

    ~ScopedTimer()
    {
        const qint64 us = m_watch.elapsedUs();
        if (m_label.isEmpty())
            qDebug().noquote() << "[ScopedTimer]" << formatUs(us);
        else
            qDebug().noquote() << "[ScopedTimer]" << m_label + ":" << formatUs(us);
    }

    // Read the elapsed time so far without waiting for destruction.
    qint64 elapsedMs() const { return m_watch.elapsedMs(); }

private:
    QString   m_label;
    Stopwatch m_watch;
};

// ---------------------------------------------------------------------------
// measureMs — run a callable once and return how long it took, in milliseconds
// (fractional). The callable's return value is ignored.
// ---------------------------------------------------------------------------
template <class F>
inline double measureMs(F &&fn)
{
    Stopwatch sw;
    std::forward<F>(fn)();
    return sw.elapsedNs() / 1.0e6;
}

// ---------------------------------------------------------------------------
// FpsCounter
// Exponentially-smoothed frames-per-second counter. Call tick() exactly once
// per rendered frame; it returns the current smoothed FPS estimate. The very
// first tick() returns 0 (no interval yet).
// ---------------------------------------------------------------------------
class FpsCounter
{
public:
    // smoothing in [0,1): 0 = use the latest frame only, closer to 1 = smoother.
    explicit FpsCounter(double smoothing = 0.9)
        : m_smoothing(smoothing < 0.0 ? 0.0 : (smoothing >= 1.0 ? 0.99 : smoothing)) {}

    double tick()
    {
        const auto now = std::chrono::steady_clock::now();
        if (!m_started) {
            m_started = true;
            m_last    = now;
            return 0.0;
        }
        const double dt = std::chrono::duration<double>(now - m_last).count();
        m_last = now;
        if (dt <= 0.0)
            return m_fps;

        const double instant = 1.0 / dt;
        m_fps = (m_fps <= 0.0)
            ? instant
            : m_smoothing * m_fps + (1.0 - m_smoothing) * instant;
        return m_fps;
    }

    double fps() const { return m_fps; }

    void reset()
    {
        m_started = false;
        m_fps     = 0.0;
    }

private:
    double m_smoothing;
    bool   m_started = false;
    double m_fps     = 0.0;
    std::chrono::steady_clock::time_point m_last;
};

// ---------------------------------------------------------------------------
// RateLimiter
// Simple time-based gate. allow() returns true only if at least intervalMs
// milliseconds have elapsed since the last time it returned true, making it
// easy to throttle logging or expensive periodic work. The first call always
// succeeds.
// ---------------------------------------------------------------------------
class RateLimiter
{
public:
    explicit RateLimiter(qint64 intervalMs)
        : m_interval(intervalMs) {}

    bool allow()
    {
        const auto now = std::chrono::steady_clock::now();
        if (!m_primed) {
            m_primed = true;
            m_last   = now;
            return true;
        }
        const qint64 elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last).count();
        if (elapsed >= m_interval) {
            m_last = now;
            return true;
        }
        return false;
    }

    void setInterval(qint64 intervalMs) { m_interval = intervalMs; }

private:
    qint64 m_interval;
    bool   m_primed = false;
    std::chrono::steady_clock::time_point m_last;
};

} // namespace QtUtil

#endif // QTUTIL_TIMER_H
