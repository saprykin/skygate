#include "Ut1OffsetResolver.hpp"
#include "EarthOrientationSampler.hpp"
#include "EphemerisTimePolicy.hpp"
#include "TimeScaleConversionWarningCode.hpp"
#include "math/TimeConstants.hpp"

#include <array>
#include <optional>
#include <string>
#include <utility>

namespace skygate::ephemeris {

namespace {

[[nodiscard]] TimeScaleConversionWarningCode::Type
timeScaleWarningFromEarthOrientationWarning(const EarthOrientationSampler::Sample::WarningCode code) noexcept
{
    switch (code) {
    case EarthOrientationSampler::Sample::WarningCode::StaleData:
        return TimeScaleConversionWarningCode::Type::EarthOrientationDataStale;
    case EarthOrientationSampler::Sample::WarningCode::PredictedData:
        return TimeScaleConversionWarningCode::Type::EarthOrientationDataPredicted;
    case EarthOrientationSampler::Sample::WarningCode::MissingData:
        return TimeScaleConversionWarningCode::Type::EarthOrientationDataMissing;
    case EarthOrientationSampler::Sample::WarningCode::EpochOutsideRange:
        return TimeScaleConversionWarningCode::Type::EpochOutsideEarthOrientationData;
    case EarthOrientationSampler::Sample::WarningCode::InvalidInput:
        return TimeScaleConversionWarningCode::Type::InvalidInput;
    case EarthOrientationSampler::Sample::WarningCode::EstimatedData:
        return TimeScaleConversionWarningCode::Type::EarthOrientationDataEstimated;
    }

    return TimeScaleConversionWarningCode::Type::EarthOrientationDataMissing;
}

void mergeEarthOrientationSampleWarnings(
    Ut1OffsetResolver::Result& result, const EarthOrientationSampler::Sample& sample
) noexcept
{
    constexpr std::array kSampleWarningCodes{
        EarthOrientationSampler::Sample::WarningCode::StaleData,
        EarthOrientationSampler::Sample::WarningCode::PredictedData,
        EarthOrientationSampler::Sample::WarningCode::MissingData,
        EarthOrientationSampler::Sample::WarningCode::EpochOutsideRange,
        EarthOrientationSampler::Sample::WarningCode::InvalidInput,
        EarthOrientationSampler::Sample::WarningCode::EstimatedData,
    };

    for (const EarthOrientationSampler::Sample::WarningCode sampleCode : kSampleWarningCodes) {
        if (sample.hasWarning(sampleCode)) {
            result.addWarning(timeScaleWarningFromEarthOrientationWarning(sampleCode));
        }
    }
}

[[nodiscard]] Ut1OffsetResolver::Result eopUt1Offset(
    const std::shared_ptr<const IEarthOrientationProvider>& earthOrientationProvider,
    const TimeScaleServiceOptions& options,
    const skygate::core::AstronomicalEpoch& utcEpoch
)
{
    const EarthOrientationSampler::Sample sample =
        EarthOrientationSampler::sample(earthOrientationProvider, utcEpoch, options.earthOrientationSampleOptions);

    Ut1OffsetResolver::Result result;
    result.diagnosticText = sample.diagnosticText;
    mergeEarthOrientationSampleWarnings(result, sample);
    if (!sample.isSuccess()) {
        result.status = TimeScaleConversionStatus::Type::Failed;
        return result;
    }

    result.offsetSeconds = sample.ut1MinusUtcSeconds;
    result.status = sample.status == EarthOrientationSampler::Sample::Status::Degraded
                        ? TimeScaleConversionStatus::Type::Degraded
                        : TimeScaleConversionStatus::Type::Valid;
    return result;
}

/// Outcome of the Delta-T fallback guards: either the estimate the UT1
/// fallback can use, or the diagnostic text that the caller reports instead.
struct DeltaTFallbackEstimate {
    std::optional<IDeltaTProvider::Estimate> estimate;
    std::string failureDiagnosticText;
};

/// The guard sequence of the Delta-T fallback: the provider has to exist and
/// its estimate has to describe the requested epoch.
[[nodiscard]] DeltaTFallbackEstimate deltaTFallbackEstimate(
    const std::shared_ptr<const IDeltaTProvider>& deltaTProvider,
    const skygate::core::AstronomicalEpoch& epoch,
    std::string providerMissingDiagnosticText
)
{
    if (deltaTProvider == nullptr) {
        return DeltaTFallbackEstimate{
            .estimate = std::nullopt,
            .failureDiagnosticText = std::move(providerMissingDiagnosticText),
        };
    }

    const IDeltaTProvider::Estimate estimate = deltaTProvider->deltaTSeconds(epoch);
    if (!estimate.isUsable() || !estimate.deltaTSeconds.has_value()) {
        return DeltaTFallbackEstimate{
            .estimate = std::nullopt,
            .failureDiagnosticText =
                estimate.diagnosticText.empty()
                    ? std::string{"UT1 conversion could not use Delta T fallback for the requested epoch."}
                    : estimate.diagnosticText,
        };
    }

    return DeltaTFallbackEstimate{.estimate = estimate, .failureDiagnosticText = {}};
}

[[nodiscard]] Ut1OffsetResolver::Result deltaTUt1Offset(
    const LeapSecondOffsetResolver& leapSecondResolver,
    const std::shared_ptr<const IDeltaTProvider>& deltaTProvider,
    const TimeScaleServiceOptions& options,
    const skygate::core::AstronomicalEpoch& utcEpoch,
    Ut1OffsetResolver::Result sourceFailure = {}
)
{
    Ut1OffsetResolver::Result result = std::move(sourceFailure);
    if (!options.allowUt1DeltaTFallback) {
        if (result.diagnosticText.empty()) {
            result.diagnosticText = "UT1 conversion requires usable Earth-orientation data.";
        }
        result.status = TimeScaleConversionStatus::Type::Failed;
        return result;
    }

    const DeltaTFallbackEstimate deltaT = deltaTFallbackEstimate(
        deltaTProvider, utcEpoch, "UT1 conversion could not use Delta T fallback because data is unavailable."
    );
    if (!deltaT.estimate.has_value()) {
        result.status = TimeScaleConversionStatus::Type::Failed;
        result.addWarning(TimeScaleConversionWarningCode::Type::DeltaTUnavailable);
        result.diagnosticText = deltaT.failureDiagnosticText;
        return result;
    }

    const IDeltaTProvider::Estimate& estimate = *deltaT.estimate;
    const LeapSecondOffsetResolver::Result utcOffset = leapSecondResolver.lookupUtcOffset(utcEpoch);
    if (!utcOffset.offsetSeconds.has_value()) {
        result.status = TimeScaleConversionStatus::Type::Failed;
        result.warningCodeMask.merge(utcOffset.warningCodeMask);
        result.diagnosticText = utcOffset.diagnosticText;
        return result;
    }

    result.offsetSeconds =
        *utcOffset.offsetSeconds + skygate::core::TimeConstants::kTtMinusTaiSeconds - *estimate.deltaTSeconds;
    result.status = TimeScaleConversionStatus::Type::Degraded;
    result.warningCodeMask.merge(utcOffset.warningCodeMask);
    result.addWarning(TimeScaleConversionWarningCode::Type::DeltaTFallbackApplied);
    if (estimate.status == IDeltaTProvider::EstimateStatus::Degraded) {
        result.addWarning(TimeScaleConversionWarningCode::Type::DeltaTFallbackApplied);
    }
    result.diagnosticText =
        estimate.diagnosticText.empty() ? "UT1 conversion used Delta T fallback metadata." : estimate.diagnosticText;
    return result;
}

[[nodiscard]] TimeScaleConversionResult utcFromUt1WithDeltaT(
    const LeapSecondOffsetResolver& leapSecondResolver,
    const std::shared_ptr<const IDeltaTProvider>& deltaTProvider,
    const TimeScaleServiceOptions& options,
    const skygate::core::AstronomicalEpoch& ut1Epoch,
    Ut1OffsetResolver::Result sourceFailure
)
{
    if (!options.allowUt1DeltaTFallback || deltaTProvider == nullptr) {
        TimeScaleConversionResult result = TimeScaleConversionResult::failure(
            ut1Epoch, skygate::core::TimeScale::Utc, std::move(sourceFailure.diagnosticText)
        );
        result.warningCodeMask = sourceFailure.warningCodeMask;
        if (deltaTProvider == nullptr) {
            result.addWarning(TimeScaleConversionWarningCode::Type::DeltaTUnavailable);
        }
        return result;
    }

    skygate::core::AstronomicalEpoch estimateEpoch = ut1Epoch;
    estimateEpoch.timeScale = skygate::core::TimeScale::Utc;
    const DeltaTFallbackEstimate deltaT = deltaTFallbackEstimate(deltaTProvider, estimateEpoch, {});
    if (!deltaT.estimate.has_value()) {
        TimeScaleConversionResult result =
            TimeScaleConversionResult::failure(ut1Epoch, skygate::core::TimeScale::Utc, deltaT.failureDiagnosticText);
        result.warningCodeMask = sourceFailure.warningCodeMask;
        result.addWarning(TimeScaleConversionWarningCode::Type::DeltaTUnavailable);
        return result;
    }

    const IDeltaTProvider::Estimate& estimate = *deltaT.estimate;
    const skygate::core::AstronomicalEpoch ttEpoch =
        ut1Epoch.addSeconds(*estimate.deltaTSeconds, skygate::core::TimeScale::Tt);
    const skygate::core::AstronomicalEpoch taiEpoch =
        ttEpoch.addSeconds(-skygate::core::TimeConstants::kTtMinusTaiSeconds, skygate::core::TimeScale::Tai);
    const LeapSecondOffsetResolver::Result taiOffset = leapSecondResolver.lookupTaiOffset(taiEpoch);
    if (!taiOffset.offsetSeconds.has_value()) {
        TimeScaleConversionResult result =
            TimeScaleConversionResult::failure(taiEpoch, skygate::core::TimeScale::Utc, taiOffset.diagnosticText);
        result.warningCodeMask = sourceFailure.warningCodeMask;
        result.warningCodeMask.merge(taiOffset.warningCodeMask);
        return result;
    }

    TimeScaleConversionResult result = TimeScaleConversionResult::success(
        taiEpoch.addSeconds(-*taiOffset.offsetSeconds, skygate::core::TimeScale::Utc),
        TimeScaleConversionStatus::Type::Degraded,
        sourceFailure.warningCodeMask | taiOffset.warningCodeMask,
        estimate.diagnosticText.empty() ? "UT1 conversion used Delta T fallback metadata." : estimate.diagnosticText
    );
    result.addWarning(TimeScaleConversionWarningCode::Type::DeltaTFallbackApplied);
    return result;
}

}  // namespace

Ut1OffsetResolver::Ut1OffsetResolver(
    std::shared_ptr<const ILeapSecondProvider> leapSecondProvider,
    std::shared_ptr<const IEarthOrientationProvider> earthOrientationProvider,
    std::shared_ptr<const IDeltaTProvider> deltaTProvider,
    TimeScaleServiceOptions options
)
    : m_earthOrientationProvider(std::move(earthOrientationProvider)), m_deltaTProvider(std::move(deltaTProvider)),
      m_leapSecondOffsetResolver(leapSecondProvider, options), m_options(options)
{
}

Ut1OffsetResolver::Result
Ut1OffsetResolver::lookupUtcToUt1Offset(const skygate::core::AstronomicalEpoch& utcEpoch) const
{
    Result result = eopUt1Offset(m_earthOrientationProvider, m_options, utcEpoch);
    if (result.isSuccess()) {
        return result;
    }

    return deltaTUt1Offset(m_leapSecondOffsetResolver, m_deltaTProvider, m_options, utcEpoch, std::move(result));
}

TimeScaleConversionResult Ut1OffsetResolver::lookupUtcFromUt1(const skygate::core::AstronomicalEpoch& ut1Epoch) const
{
    skygate::core::AstronomicalEpoch utcEpoch = ut1Epoch;
    utcEpoch.timeScale = skygate::core::TimeScale::Utc;
    Result lookup;
    for (int iteration = 0; iteration < EphemerisTimePolicy::kUt1InverseIterations; ++iteration) {
        lookup = eopUt1Offset(m_earthOrientationProvider, m_options, utcEpoch);
        if (!lookup.offsetSeconds.has_value()) {
            return utcFromUt1WithDeltaT(
                m_leapSecondOffsetResolver, m_deltaTProvider, m_options, ut1Epoch, std::move(lookup)
            );
        }
        utcEpoch = ut1Epoch.addSeconds(-*lookup.offsetSeconds, skygate::core::TimeScale::Utc);
    }

    TimeScaleConversionResult result =
        TimeScaleConversionResult::success(utcEpoch, lookup.status, lookup.warningCodeMask, lookup.diagnosticText);
    return result;
}

}  // namespace skygate::ephemeris
