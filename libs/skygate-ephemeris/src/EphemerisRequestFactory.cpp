#include "skygate/ephemeris/EphemerisRequestFactory.hpp"

#include "skygate/core/UtcTimeCodec.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>

namespace skygate::ephemeris {

bool EphemerisRequestFactory::hasExplicitEpoch(const AstronomicalEpoch& epoch) noexcept
{
    return std::isfinite(epoch.julianDatePart1) && std::isfinite(epoch.julianDatePart2)
           && (epoch.julianDatePart1 != 0.0 || epoch.julianDatePart2 != 0.0);
}

AstronomicalEpoch EphemerisRequestFactory::epochFromUtcTime(const core::UtcTimePoint& utcTime) noexcept
{
    const double julianDay =
        core::UtcTimeCodec::secondsSinceEpochDouble(utcTime) / static_cast<double>(detail::kSecondsPerDay)
        + detail::kJulianDateUnixEpoch;
    const double julianDatePart1 = std::floor(julianDay);
    return AstronomicalEpoch{
        .julianDatePart1 = julianDatePart1,
        .julianDatePart2 = julianDay - julianDatePart1,
        .timeScale = TimeScale::Utc,
    };
}

core::UtcTimePoint EphemerisRequestFactory::utcTimeFromEpoch(const AstronomicalEpoch& epoch) noexcept
{
    const AstronomicalEpoch normalizedEpoch = normalizedAstronomicalEpoch(epoch);
    const double julianDay = normalizedEpoch.julianDatePart1 + normalizedEpoch.julianDatePart2;
    const double epochMicros = std::round(
        (julianDay - detail::kJulianDateUnixEpoch) * static_cast<double>(detail::kSecondsPerDay) * 1'000'000.0
    );
    return core::UtcTimeCodec::fromEpochMicros(static_cast<std::int64_t>(epochMicros));
}

core::SkyContext EphemerisRequestFactory::contextFromRequest(const EphemerisRequest& request) noexcept
{
    core::SkyContext context = request.context;
    if (request.epoch.timeScale == TimeScale::Utc && hasExplicitEpoch(request.epoch)) {
        context.utcTime = utcTimeFromEpoch(request.epoch);
    }

    return context;
}

EphemerisRequest
EphemerisRequestFactory::fromContext(const core::SkyContext& context, const EphemerisEngineOptions& options) noexcept
{
    return EphemerisRequest{
        .epoch = epochFromUtcTime(context.utcTime),
        .context = context,
        .options = options,
    };
}

EphemerisRequest
EphemerisRequestFactory::atUtcTime(const EphemerisRequest& baseRequest, const core::UtcTimePoint& utcTime) noexcept
{
    EphemerisRequest request = baseRequest;
    const double offsetSeconds = std::chrono::duration<double>(utcTime - baseRequest.context.utcTime).count();
    request.context.utcTime = utcTime;
    request.epoch = normalizedAstronomicalEpoch(
        AstronomicalEpoch{
            .julianDatePart1 = baseRequest.epoch.julianDatePart1,
            .julianDatePart2 =
                baseRequest.epoch.julianDatePart2 + offsetSeconds / static_cast<double>(detail::kSecondsPerDay),
            .timeScale = baseRequest.epoch.timeScale,
        }
    );
    return request;
}

}  // namespace skygate::ephemeris
