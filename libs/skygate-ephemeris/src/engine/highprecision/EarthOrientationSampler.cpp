#include "EarthOrientationSampler.hpp"

#include <algorithm>
#include <utility>

namespace skygate::ephemeris {

namespace {

[[nodiscard]] bool epochInRange(const AstronomicalEpoch& epoch, const EphemerisDateRange& range) noexcept
{
    const double key = epoch.sortKey();
    return key >= range.start.sortKey() && key <= range.end.sortKey();
}

[[nodiscard]] bool epochAfter(const AstronomicalEpoch& epoch, const AstronomicalEpoch& boundary) noexcept
{
    return epoch.sortKey() > boundary.sortKey();
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
    const AstronomicalEpoch& utcEpoch,
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
missingDataSample(const AstronomicalEpoch& utcEpoch, const EarthOrientationSampler::Options& options)
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
    sample.addWarning(EarthOrientationSampler::Sample::WarningCode::MissingData);
    sample.diagnosticText = "Earth-orientation data used a degraded zero-value fallback.";
    return sample;
}

[[nodiscard]] EarthOrientationSampler::Sample
sampleFromEntry(const AstronomicalEpoch& requestedEpoch, const IEarthOrientationProvider::TableEntry& entry)
{
    EarthOrientationSampler::Sample sample;
    sample.requestedUtcEpoch = requestedEpoch;
    sample.ut1MinusUtcSeconds = entry.ut1MinusUtcSeconds;
    sample.polarMotionXArcseconds = entry.polarMotionXArcseconds;
    sample.polarMotionYArcseconds = entry.polarMotionYArcseconds;
    sample.predicted = entry.predicted;
    sample.estimated = entry.estimated;
    sample.status = EarthOrientationSampler::Sample::Status::Valid;
    sample.diagnosticText = "Earth-orientation sample resolved.";
    return sample;
}

[[nodiscard]] EarthOrientationSampler::Sample interpolateSamples(
    const AstronomicalEpoch& utcEpoch,
    const IEarthOrientationProvider::TableEntry& lower,
    const IEarthOrientationProvider::TableEntry& upper
)
{
    const double lowerKey = lower.effectiveUtcEpoch.sortKey();
    const double upperKey = upper.effectiveUtcEpoch.sortKey();
    const double denominator = upperKey - lowerKey;
    if (denominator <= 0.0) {
        return failedSample(
            utcEpoch,
            EarthOrientationSampler::Sample::WarningCode::InvalidInput,
            "Earth-orientation rows do not form a valid interpolation interval."
        );
    }

    const double ratio = (utcEpoch.sortKey() - lowerKey) / denominator;
    EarthOrientationSampler::Sample sample;
    sample.requestedUtcEpoch = utcEpoch;
    sample.ut1MinusUtcSeconds =
        lower.ut1MinusUtcSeconds + (upper.ut1MinusUtcSeconds - lower.ut1MinusUtcSeconds) * ratio;
    sample.polarMotionXArcseconds =
        lower.polarMotionXArcseconds + (upper.polarMotionXArcseconds - lower.polarMotionXArcseconds) * ratio;
    sample.polarMotionYArcseconds =
        lower.polarMotionYArcseconds + (upper.polarMotionYArcseconds - lower.polarMotionYArcseconds) * ratio;
    sample.predicted = lower.predicted || upper.predicted;
    sample.estimated = lower.estimated || upper.estimated;
    sample.status = EarthOrientationSampler::Sample::Status::Valid;
    sample.diagnosticText = "Earth-orientation sample interpolated.";
    return sample;
}

void applyDataWarnings(
    EarthOrientationSampler::Sample& sample,
    const IEarthOrientationProvider::DataInfo& info,
    const AstronomicalEpoch& utcEpoch,
    const EarthOrientationSampler::Options& options
) noexcept
{
    if (info.status == IEarthOrientationProvider::DataStatus::Stale
        || (info.expiresAt.has_value() && epochAfter(utcEpoch, *info.expiresAt))) {
        addSampleWarning(sample, EarthOrientationSampler::Sample::WarningCode::StaleData);
    }

    if (info.status == IEarthOrientationProvider::DataStatus::Estimated || sample.estimated) {
        sample.estimated = true;
        addSampleWarning(sample, EarthOrientationSampler::Sample::WarningCode::EstimatedData);
    }

    if (sample.predicted || (info.predictionRange.has_value() && epochInRange(utcEpoch, *info.predictionRange))) {
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
    const AstronomicalEpoch& utcEpoch,
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

    if (provider == nullptr || !provider->dataInfo().isUsable() || provider->entries().empty()) {
        return missingDataSample(utcEpoch, options);
    }

    const std::span<const IEarthOrientationProvider::TableEntry> entries = provider->entries();
    const double requestedKey = utcEpoch.sortKey();
    const auto lowerBound = std::ranges::lower_bound(entries, requestedKey, {}, [](const auto& entry) {
        return entry.effectiveUtcEpoch.sortKey();
    });

    EarthOrientationSampler::Sample sample;
    if (lowerBound != entries.end() && lowerBound->effectiveUtcEpoch.sortKey() == requestedKey) {
        sample = sampleFromEntry(utcEpoch, *lowerBound);
    } else if (lowerBound == entries.begin()) {
        if (!options.allowOutOfRangeNearestSampleFallback) {
            return failedSample(
                utcEpoch,
                EarthOrientationSampler::Sample::WarningCode::EpochOutsideRange,
                "Requested epoch is before the first Earth-orientation row."
            );
        }
        sample = sampleFromEntry(utcEpoch, entries.front());
        addSampleWarning(sample, EarthOrientationSampler::Sample::WarningCode::EpochOutsideRange);
        sample.diagnosticText = "Earth-orientation sample used the first available row as a degraded fallback.";
    } else if (lowerBound == entries.end()) {
        if (!options.allowOutOfRangeNearestSampleFallback) {
            return failedSample(
                utcEpoch,
                EarthOrientationSampler::Sample::WarningCode::EpochOutsideRange,
                "Requested epoch is after the last Earth-orientation row."
            );
        }
        sample = sampleFromEntry(utcEpoch, entries.back());
        addSampleWarning(sample, EarthOrientationSampler::Sample::WarningCode::EpochOutsideRange);
        sample.diagnosticText = "Earth-orientation sample used the last available row as a degraded fallback.";
    } else {
        sample = interpolateSamples(utcEpoch, *(lowerBound - 1), *lowerBound);
    }

    applyDataWarnings(sample, provider->dataInfo(), utcEpoch, options);
    if (sample.status == EarthOrientationSampler::Sample::Status::Degraded
        && sample.diagnosticText == "Earth-orientation sample resolved.") {
        sample.diagnosticText = "Earth-orientation sample resolved with degraded metadata.";
    }
    if (sample.status == EarthOrientationSampler::Sample::Status::Degraded
        && sample.diagnosticText == "Earth-orientation sample interpolated.") {
        sample.diagnosticText = "Earth-orientation sample interpolated with degraded metadata.";
    }
    return sample;
}

EarthOrientationSampler::Sample EarthOrientationSampler::sample(
    const std::shared_ptr<const IEarthOrientationProvider>& provider,
    const AstronomicalEpoch& utcEpoch,
    const EarthOrientationSampler::Options& options
)
{
    return EarthOrientationSampler::sample(provider.get(), utcEpoch, options);
}

EarthOrientationSampler::Sample
EarthOrientationSampler::sample(const IEarthOrientationProvider* provider, const AstronomicalEpoch& utcEpoch)
{
    return sample(provider, utcEpoch, Options{});
}

EarthOrientationSampler::Sample EarthOrientationSampler::sample(
    const std::shared_ptr<const IEarthOrientationProvider>& provider, const AstronomicalEpoch& utcEpoch
)
{
    return sample(provider.get(), utcEpoch, Options{});
}

}  // namespace skygate::ephemeris
