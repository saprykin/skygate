#include "EphemerisRequestFactory.hpp"
#include "math/TimeConstants.hpp"
#include "time/AstronomicalEpoch.hpp"
#include "time/EpochCodec.hpp"

#include <chrono>

namespace skygate::ephemeris {

using core::TimeConstants;

core::SkyContext EphemerisRequestFactory::contextFromRequest(const EphemerisRequest& request) noexcept
{
    core::SkyContext context = request.context;
    if (request.epoch.timeScale == TimeScale::Utc && hasExplicitEpoch(request.epoch)) {
        context.utcTime = EpochCodec::utcTimeFromEpoch(request.epoch);
    }

    return context;
}

EphemerisRequest
EphemerisRequestFactory::fromContext(const core::SkyContext& context, const EphemerisEngineOptions& options) noexcept
{
    return EphemerisRequest{
        .epoch = EpochCodec::epochFromUtcTime(context.utcTime),
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
            .julianDatePart2 = baseRequest.epoch.julianDatePart2 + offsetSeconds / TimeConstants::kSecondsPerDay,
            .timeScale = baseRequest.epoch.timeScale,
        }
    );
    return request;
}

}  // namespace skygate::ephemeris
