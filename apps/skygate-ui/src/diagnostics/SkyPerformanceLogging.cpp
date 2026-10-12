#include "SkyPerformanceLogging.hpp"

namespace skygate::ui {
namespace {

constexpr double kNanosecondsPerMillisecond = 1000000.0;

}  // namespace

Q_LOGGING_CATEGORY(skygatePerfLog, "skygate.perf")

bool SkyPerformanceLogging::enabled() noexcept
{
    static const bool loggingEnabled = qEnvironmentVariableIsSet("SKYGATE_PERF_LOG");
    return loggingEnabled;
}

void SkyPerformanceLogging::startTimer(QElapsedTimer& timer)
{
    if (enabled()) {
        timer.start();
    }
}

qint64 SkyPerformanceLogging::elapsedNanoseconds(const QElapsedTimer& timer) noexcept
{
    return enabled() ? timer.nsecsElapsed() : 0;
}

double SkyPerformanceLogging::milliseconds(const qint64 nanoseconds) noexcept
{
    return static_cast<double>(nanoseconds) / kNanosecondsPerMillisecond;
}

double SkyPerformanceLogging::elapsedMilliseconds(const QElapsedTimer& timer) noexcept
{
    return milliseconds(elapsedNanoseconds(timer));
}

}  // namespace skygate::ui
