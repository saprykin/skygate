#include "EphemerisRequestFactory.hpp"
#include "math/TimeConstants.hpp"
#include "time/AstronomicalEpoch.hpp"
#include "time/EpochCodec.hpp"

#include <chrono>

namespace skygate::ephemeris {

using core::TimeConstants;

core::ObservationContext EphemerisRequestFactory::contextFromRequest(const EphemerisRequest& request) noexcept
{
    core::ObservationContext context = request.context;
    if (request.epoch.timeScale == TimeScale::Utc && request.epoch.hasExplicit()) {
        context.utcTime = EpochCodec::utcTimeFromEpoch(request.epoch);
    }

    return context;
}

EphemerisRequest EphemerisRequestFactory::requestFromContext(
    const core::ObservationContext& context, const EphemerisEngineOptions& options
) noexcept
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
    request.epoch =
        AstronomicalEpoch{
            .julianDatePart1 = baseRequest.epoch.julianDatePart1,
            .julianDatePart2 = baseRequest.epoch.julianDatePart2 + offsetSeconds / TimeConstants::kSecondsPerDay,
            .timeScale = baseRequest.epoch.timeScale,
        }
            .normalized();
    return request;
}

}  // namespace skygate::ephemeris
