#include "SkyPerformanceLogging.hpp"

namespace skygate::ui {
namespace {

constexpr double kNanosecondsPerMillisecond = 1000000.0;

}  // namespace

Q_LOGGING_CATEGORY(skygatePerfLog, "skygate.perf")

bool performanceLoggingEnabled() noexcept
{
    static const bool enabled = qEnvironmentVariableIsSet("SKYGATE_PERF_LOG");
    return enabled;
}

void startPerformanceTimer(QElapsedTimer& timer)
{
    if (performanceLoggingEnabled()) {
        timer.start();
    }
}

qint64 performanceElapsedNanoseconds(const QElapsedTimer& timer) noexcept
{
    return performanceLoggingEnabled() ? timer.nsecsElapsed() : 0;
}

double performanceMilliseconds(const qint64 nanoseconds) noexcept
{
    return static_cast<double>(nanoseconds) / kNanosecondsPerMillisecond;
}

double performanceElapsedMilliseconds(const QElapsedTimer& timer) noexcept
{
    return performanceMilliseconds(performanceElapsedNanoseconds(timer));
}

}  // namespace skygate::ui
