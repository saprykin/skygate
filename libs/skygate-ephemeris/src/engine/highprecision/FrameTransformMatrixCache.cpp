#include "FrameTransformMatrixCache.hpp"
#include "ErfaAstrometry.hpp"
#include "EpochScaleResolver.hpp"
#include "math/MathConstants.hpp"

#include <optional>
#include <utility>

namespace skygate::ephemeris::highprecision {

FrameTransformMatrixCache::FrameTransformMatrixCache(
    const skygate::core::AstronomicalEpoch epoch,
    const skygate::ephemeris::ITimeScaleService* timeScaleService,
    const skygate::ephemeris::IEarthOrientationProvider* earthOrientationProvider
)
    : m_epoch(epoch), m_timeScaleService(timeScaleService), m_earthOrientationProvider(earthOrientationProvider)
{
}

void FrameTransformMatrixCache::Entry::recordMatrix(std::optional<skygate::core::Matrix3x3> value) noexcept
{
    matrix = std::move(value);
    if (!matrix.has_value()) {
        EphemerisMetadataMerger::markComputationFailed(metadata);
    }
}

void FrameTransformMatrixCache::Entry::mergeMetadataInto(EphemerisEngineQueryResult& target) const noexcept
{
    EphemerisMetadataMerger::merge(target, metadata, FrameTransformMatrixCache::cachedMetadataMergeOptions());
}

std::optional<skygate::core::Matrix3x3>
FrameTransformMatrixCache::matrix(const MatrixKind kind, EphemerisEngineQueryResult& metadata)
{
    const std::size_t index = static_cast<std::size_t>(kind);
    if (index >= m_entries.size()) {
        EphemerisMetadataMerger::markComputationFailed(metadata);
        return std::nullopt;
    }

    std::optional<Entry>& entry = m_entries[index];
    if (!entry.has_value()) {
        entry.emplace();
        computeEntry(kind, *entry);
    }

    entry->mergeMetadataInto(metadata);
    return entry->matrix;
}

void FrameTransformMatrixCache::computeEntry(const MatrixKind kind, Entry& entry)
{
    switch (kind) {
    case MatrixKind::CelestialIntermediate: {
        const std::optional<skygate::core::AstronomicalEpoch> ttEpoch =
            epochInScale(skygate::core::TimeScale::Tt, entry.metadata);
        if (ttEpoch.has_value()) {
            entry.recordMatrix(ErfaAstrometry::celestialToIntermediateMatrix06A(*ttEpoch));
        }
        return;
    }
    case MatrixKind::EarthRotation: {
        const std::optional<skygate::core::AstronomicalEpoch> universalTime1 = ut1Epoch(entry.metadata);
        if (universalTime1.has_value()) {
            entry.recordMatrix(ErfaAstrometry::earthRotationMatrix00(*universalTime1));
        }
        return;
    }
    case MatrixKind::PolarMotion: {
        const std::optional<skygate::core::AstronomicalEpoch> ttEpoch =
            epochInScale(skygate::core::TimeScale::Tt, entry.metadata);
        const std::optional<EarthOrientationSampler::Sample> earthOrientationSample = earthOrientation(entry.metadata);
        if (!ttEpoch.has_value() || !earthOrientationSample.has_value()) {
            return;
        }

        const std::optional<double> tioLocator = ErfaAstrometry::tioLocatorS00(*ttEpoch);
        if (tioLocator.has_value()) {
            const double polarMotionXRadians =
                earthOrientationSample->polarMotionXArcseconds * skygate::core::MathConstants::kArcsecondsToRadians;
            const double polarMotionYRadians =
                earthOrientationSample->polarMotionYArcseconds * skygate::core::MathConstants::kArcsecondsToRadians;
            entry.recordMatrix(
                ErfaAstrometry::polarMotionMatrix00(polarMotionXRadians, polarMotionYRadians, *tioLocator)
            );
        } else {
            entry.recordMatrix(std::nullopt);
        }
        return;
    }
    case MatrixKind::PrecessionNutation: {
        const std::optional<skygate::core::AstronomicalEpoch> ttEpoch =
            epochInScale(skygate::core::TimeScale::Tt, entry.metadata);
        if (ttEpoch.has_value()) {
            entry.recordMatrix(ErfaAstrometry::precessionNutationMatrix06A(*ttEpoch));
        }
        return;
    }
    }

    EphemerisMetadataMerger::markComputationFailed(entry.metadata);
}

std::optional<skygate::core::AstronomicalEpoch> FrameTransformMatrixCache::epochInScale(
    const skygate::core::TimeScale targetScale, EphemerisEngineQueryResult& metadata
)
{
    if (!m_epoch.isFinite()) {
        EphemerisMetadataMerger::markComputationFailed(metadata);
        return std::nullopt;
    }

    // A same-scale epoch needs no conversion and no conversion slot, so it is
    // resolved before the check that the target scale is cacheable.
    if (m_epoch.timeScale == targetScale) {
        return m_epoch.normalized();
    }

    std::optional<TimeScaleConversionResult>* const conversionCache = conversionCacheFor(targetScale);
    if (conversionCache == nullptr) {
        EphemerisMetadataMerger::markTimeScaleUnavailable(metadata, EphemerisMetadataFailurePolicy::MarkFailed);
        return std::nullopt;
    }

    return EpochScaleResolver::resolveInScale(
        metadata, m_epoch, targetScale, m_timeScaleService, EphemerisMetadataFailurePolicy::MarkFailed, conversionCache
    );
}

std::optional<EarthOrientationSampler::Sample>
FrameTransformMatrixCache::earthOrientation(EphemerisEngineQueryResult& metadata)
{
    return EphemerisMetadataMerger::prepareEarthOrientation(
        m_epoch,
        m_timeScaleService,
        m_earthOrientationProvider,
        metadata,
        EphemerisMetadataFailurePolicy::MarkFailed,
        EarthOrientationSampler::Options::tolerantFallback(),
        &m_earthOrientationCache
    );
}

std::optional<skygate::core::AstronomicalEpoch>
FrameTransformMatrixCache::ut1Epoch(EphemerisEngineQueryResult& metadata)
{
    if (m_epoch.timeScale == skygate::core::TimeScale::Ut1 || m_earthOrientationProvider == nullptr) {
        return epochInScale(skygate::core::TimeScale::Ut1, metadata);
    }

    const std::optional<EarthOrientationSampler::Sample> sample = earthOrientation(metadata);
    if (!sample.has_value()) {
        return std::nullopt;
    }

    return sample->requestedUtcEpoch.addSeconds(sample->ut1MinusUtcSeconds, skygate::core::TimeScale::Ut1);
}

std::optional<TimeScaleConversionResult>*
FrameTransformMatrixCache::conversionCacheFor(const skygate::core::TimeScale targetScale)
{
    switch (targetScale) {
    case skygate::core::TimeScale::Tt:
        return &m_ttConversion;
    case skygate::core::TimeScale::Utc:
        return &m_earthOrientationCache.utcConversion;
    case skygate::core::TimeScale::Ut1:
        return &m_ut1Conversion;
    case skygate::core::TimeScale::Tai:
    case skygate::core::TimeScale::Tdb:
        return nullptr;
    }

    return nullptr;
}

}  // namespace skygate::ephemeris::highprecision
