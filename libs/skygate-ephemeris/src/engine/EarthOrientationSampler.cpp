#include "EarthOrientationSampler.hpp"
#include "EphemerisDataStaleness.hpp"
#include "EphemerisTimePolicy.hpp"
#include "TableInterpolation.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace skygate::ephemeris {

EphemerisEngineWarning::Code EarthOrientationSampler::Sample::engineWarningCode(const WarningCode code) noexcept
{
    switch (code) {
    case WarningCode::StaleData:
        return EphemerisEngineWarning::Code::EarthOrientationStaleData;
    case WarningCode::PredictedData:
        return EphemerisEngineWarning::Code::EarthOrientationPredictedData;
    case WarningCode::MissingData:
        return EphemerisEngineWarning::Code::EarthOrientationMissingData;
    case WarningCode::EpochOutsideRange:
        return EphemerisEngineWarning::Code::EarthOrientationEpochOutsideRange;
    case WarningCode::InvalidInput:
        return EphemerisEngineWarning::Code::EarthOrientationInvalidInput;
    case WarningCode::EstimatedData:
        return EphemerisEngineWarning::Code::EarthOrientationEstimatedData;
    }

    return EphemerisEngineWarning::Code::AccuracyDegraded;
}

std::string EarthOrientationSampler::Sample::diagnosticTextFor(
    const Origin origin, const Status status, const bool nearestRowUsedFirstRow
)
{
    switch (origin) {
    case Origin::ExactRow:
        return status == Status::Degraded ? "Earth-orientation sample resolved with degraded metadata."
                                          : "Earth-orientation sample resolved.";
    case Origin::Interpolated:
        return status == Status::Degraded ? "Earth-orientation sample interpolated with degraded metadata."
                                          : "Earth-orientation sample interpolated.";
    case Origin::NearestRow:
        return nearestRowUsedFirstRow ? "Earth-orientation sample used the first available row as a degraded fallback."
                                      : "Earth-orientation sample used the last available row as a degraded fallback.";
    case Origin::ZeroFallback:
        return "Earth-orientation data used a degraded zero-value fallback.";
    }

    return {};
}

namespace {

[[nodiscard]] bool sameEpochKey(const double lhs, const double rhs) noexcept
{
    return std::abs(lhs - rhs) <= EphemerisTimePolicy::kEpochKeyEqualityToleranceDays;
}

void addSampleWarning(
    EarthOrientationSampler::Sample& sample, const EarthOrientationSampler::Sample::WarningCode code
) noexcept
{
    if (sample.status == EarthOrientationSampler::Sample::Status::Valid) {
        sample.status = EarthOrientationSampler::Sample::Status::Degraded;
    }
    sample.addWarning(code);
}

[[nodiscard]] EarthOrientationSampler::Sample failedSample(
    const skygate::core::AstronomicalEpoch& utcEpoch,
    const EarthOrientationSampler::Sample::WarningCode code,
    std::string diagnosticText
)
{
    EarthOrientationSampler::Sample sample;
    sample.requestedUtcEpoch = utcEpoch;
    sample.status = EarthOrientationSampler::Sample::Status::Failed;
    sample.diagnosticText = std::move(diagnosticText);
    sample.addWarning(code);
    return sample;
}

[[nodiscard]] EarthOrientationSampler::Sample
missingDataSample(const skygate::core::AstronomicalEpoch& utcEpoch, const EarthOrientationSampler::Options& options)
{
    if (!options.allowMissingDataZeroFallback) {
        return failedSample(
            utcEpoch,
            EarthOrientationSampler::Sample::WarningCode::MissingData,
            "Earth-orientation data is unavailable for the requested epoch."
        );
    }

    EarthOrientationSampler::Sample sample;
    sample.requestedUtcEpoch = utcEpoch;
    sample.status = EarthOrientationSampler::Sample::Status::Degraded;
    sample.origin = EarthOrientationSampler::Sample::Origin::ZeroFallback;
    sample.addWarning(EarthOrientationSampler::Sample::WarningCode::MissingData);
    return sample;
}

[[nodiscard]] EarthOrientationSampler::Sample sampleFromEntry(
    const skygate::core::AstronomicalEpoch& requestedEpoch,
    const IEarthOrientationProvider::TableEntry& entry,
    const EarthOrientationSampler::Sample::Origin origin
)
{
    EarthOrientationSampler::Sample sample;
    sample.requestedUtcEpoch = requestedEpoch;
    sample.ut1MinusUtcSeconds = entry.ut1MinusUtcSeconds;
    sample.polarMotionXArcseconds = entry.polarMotionXArcseconds;
    sample.polarMotionYArcseconds = entry.polarMotionYArcseconds;
    sample.predicted = entry.predicted;
    sample.estimated = entry.estimated;
    sample.status = EarthOrientationSampler::Sample::Status::Valid;
    sample.origin = origin;
    return sample;
}

[[nodiscard]] EarthOrientationSampler::Sample interpolateSamples(
    const skygate::core::AstronomicalEpoch& utcEpoch,
    const IEarthOrientationProvider::TableEntry& lower,
    const IEarthOrientationProvider::TableEntry& upper
)
{
    const std::optional<double> ratio = TableInterpolation::ratio(
        utcEpoch.sortKey(), lower.effectiveUtcEpoch.sortKey(), upper.effectiveUtcEpoch.sortKey()
    );
    if (!ratio.has_value()) {
        return failedSample(
            utcEpoch,
            EarthOrientationSampler::Sample::WarningCode::InvalidInput,
            "Earth-orientation rows do not form a valid interpolation interval."
        );
    }

    EarthOrientationSampler::Sample sample;
    sample.requestedUtcEpoch = utcEpoch;
    sample.ut1MinusUtcSeconds =
        lower.ut1MinusUtcSeconds + (upper.ut1MinusUtcSeconds - lower.ut1MinusUtcSeconds) * *ratio;
    sample.polarMotionXArcseconds =
        lower.polarMotionXArcseconds + (upper.polarMotionXArcseconds - lower.polarMotionXArcseconds) * *ratio;
    sample.polarMotionYArcseconds =
        lower.polarMotionYArcseconds + (upper.polarMotionYArcseconds - lower.polarMotionYArcseconds) * *ratio;
    sample.predicted = lower.predicted || upper.predicted;
    sample.estimated = lower.estimated || upper.estimated;
    sample.status = EarthOrientationSampler::Sample::Status::Valid;
    sample.origin = EarthOrientationSampler::Sample::Origin::Interpolated;
    return sample;
}

void applyDataWarnings(
    EarthOrientationSampler::Sample& sample,
    const IEarthOrientationProvider::DataInfo& info,
    const skygate::core::AstronomicalEpoch& utcEpoch,
    const EarthOrientationSampler::Options& options
) noexcept
{
    if (info.status == IEarthOrientationProvider::DataStatus::Stale
        || EphemerisDataStaleness::isStale(utcEpoch, info.expiresAt)) {
        addSampleWarning(sample, EarthOrientationSampler::Sample::WarningCode::StaleData);
    }

    if (info.status == IEarthOrientationProvider::DataStatus::Estimated || sample.estimated) {
        sample.estimated = true;
        addSampleWarning(sample, EarthOrientationSampler::Sample::WarningCode::EstimatedData);
    }

    if (sample.predicted || (info.predictionRange.has_value() && info.predictionRange->contains(utcEpoch))) {
        sample.predicted = true;
        if (options.degradePredictedData) {
            addSampleWarning(sample, EarthOrientationSampler::Sample::WarningCode::PredictedData);
        } else {
            sample.addWarning(EarthOrientationSampler::Sample::WarningCode::PredictedData);
        }
    }
}

}  // namespace

EarthOrientationSampler::Sample EarthOrientationSampler::sample(
    const IEarthOrientationProvider* provider,
    const skygate::core::AstronomicalEpoch& utcEpoch,
    const EarthOrientationSampler::Options& options
)
{
    if (!utcEpoch.isFiniteUtc()) {
        return failedSample(
            utcEpoch,
            EarthOrientationSampler::Sample::WarningCode::InvalidInput,
            "Earth-orientation sampling requires a finite UTC epoch."
        );
    }

    EarthOrientationSampler::Sample sample;
    // Names the edge an out-of-range fallback reused; other origins ignore it.
    bool nearestRowUsedFirstRow = false;

    if (provider == nullptr || !provider->dataInfo().isUsable() || provider->entries().empty()) {
        sample = missingDataSample(utcEpoch, options);
        if (!sample.isSuccess()) {
            return sample;
        }
    } else {
        const std::span<const IEarthOrientationProvider::TableEntry> entries = provider->entries();
        const double requestedKey = utcEpoch.sortKey();
        const auto lowerBound = std::ranges::lower_bound(entries, requestedKey, {}, [](const auto& entry) {
            return entry.effectiveUtcEpoch.sortKey();
        });

        if (lowerBound != entries.end() && sameEpochKey(lowerBound->effectiveUtcEpoch.sortKey(), requestedKey)) {
            sample = sampleFromEntry(utcEpoch, *lowerBound, EarthOrientationSampler::Sample::Origin::ExactRow);
        } else if (lowerBound == entries.begin()) {
            if (!options.allowOutOfRangeNearestSampleFallback) {
                return failedSample(
                    utcEpoch,
                    EarthOrientationSampler::Sample::WarningCode::EpochOutsideRange,
                    "Requested epoch is before the first Earth-orientation row."
                );
            }
            sample = sampleFromEntry(utcEpoch, entries.front(), EarthOrientationSampler::Sample::Origin::NearestRow);
            nearestRowUsedFirstRow = true;
            addSampleWarning(sample, EarthOrientationSampler::Sample::WarningCode::EpochOutsideRange);
        } else if (lowerBound == entries.end()) {
            if (!options.allowOutOfRangeNearestSampleFallback) {
                return failedSample(
                    utcEpoch,
                    EarthOrientationSampler::Sample::WarningCode::EpochOutsideRange,
                    "Requested epoch is after the last Earth-orientation row."
                );
            }
            sample = sampleFromEntry(utcEpoch, entries.back(), EarthOrientationSampler::Sample::Origin::NearestRow);
            addSampleWarning(sample, EarthOrientationSampler::Sample::WarningCode::EpochOutsideRange);
        } else {
            sample = interpolateSamples(utcEpoch, *(lowerBound - 1), *lowerBound);
        }

        applyDataWarnings(sample, provider->dataInfo(), utcEpoch, options);
    }

    if (sample.isSuccess()) {
        sample.diagnosticText =
            EarthOrientationSampler::Sample::diagnosticTextFor(sample.origin, sample.status, nearestRowUsedFirstRow);
    }
    return sample;
}

EarthOrientationSampler::Sample EarthOrientationSampler::sample(
    const std::shared_ptr<const IEarthOrientationProvider>& provider,
    const skygate::core::AstronomicalEpoch& utcEpoch,
    const EarthOrientationSampler::Options& options
)
{
    return EarthOrientationSampler::sample(provider.get(), utcEpoch, options);
}

EarthOrientationSampler::Sample EarthOrientationSampler::sample(
    const IEarthOrientationProvider* provider, const skygate::core::AstronomicalEpoch& utcEpoch
)
{
    return sample(provider, utcEpoch, Options{});
}

EarthOrientationSampler::Sample EarthOrientationSampler::sample(
    const std::shared_ptr<const IEarthOrientationProvider>& provider, const skygate::core::AstronomicalEpoch& utcEpoch
)
{
    return sample(provider.get(), utcEpoch, Options{});
}

}  // namespace skygate::ephemeris
