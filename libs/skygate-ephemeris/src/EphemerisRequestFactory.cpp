#include "EphemerisRequestFactory.hpp"
#include "time/EpochCodec.hpp"

namespace skygate::ephemeris {
skygate::core::ObservationContext EphemerisRequestFactory::contextFromRequest(const EphemerisRequest& request) noexcept
{
    skygate::core::ObservationContext context = request.context;
    if (request.epoch.timeScale == skygate::core::TimeScale::Utc && request.epoch.hasExplicit()) {
        context.utcTime = skygate::core::EpochCodec::utcTimeFromEpoch(request.epoch);
    }

    return context;
}

EphemerisRequest EphemerisRequestFactory::requestFromContext(
    const skygate::core::ObservationContext& context, const EphemerisEngineOptions& options
) noexcept
{
    return EphemerisRequest{
        .epoch = skygate::core::EpochCodec::epochFromUtcTime(context.utcTime),
        .context = context,
        .options = options,
    };
}

EphemerisRequest EphemerisRequestFactory::atUtcTime(
    const EphemerisRequest& baseRequest, const skygate::core::UtcTimePoint& utcTime
) noexcept
{
    EphemerisRequest request = baseRequest;
    request.context.utcTime = utcTime;

    // The requested instant is always a UTC instant. Derive the request epoch
    // from the UTC time point instead of adding a UTC interval to a base epoch
    // that may be expressed in a non-UTC scale (TT/TDB/UT1).
    request.epoch = skygate::core::EpochCodec::epochFromUtcTime(utcTime);
    return request;
}

}  // namespace skygate::ephemeris
