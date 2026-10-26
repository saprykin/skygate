#pragma once

#include "EphemerisMetadataMerger.hpp"
#include "FrameTransformStageCatalog.hpp"
#include "engine/EphemerisEngineQueryResult.hpp"
#include "math/Matrix3x3.hpp"
#include "time/AstronomicalEpoch.hpp"

#include <array>
#include <cstddef>
#include <optional>

namespace skygate::ephemeris {
class IEarthOrientationProvider;
class ITimeScaleService;
}  // namespace skygate::ephemeris

namespace skygate::ephemeris::highprecision {

// Lazily computes and keeps the ERFA matrices for one transform epoch. Entries
// are keyed by the matrix kind a stage row names, so a kind used more than once
// during a transform is computed once and its metadata reused.
class FrameTransformMatrixCache final {
public:
    using MatrixKind = FrameTransformStageCatalog::MatrixKind;

    FrameTransformMatrixCache(
        skygate::core::AstronomicalEpoch epoch,
        const skygate::ephemeris::ITimeScaleService* timeScaleService,
        const skygate::ephemeris::IEarthOrientationProvider* earthOrientationProvider
    );

    [[nodiscard]] std::optional<skygate::core::Matrix3x3> matrix(MatrixKind kind, EphemerisEngineQueryResult& metadata);

private:
    struct Entry {
        std::optional<skygate::core::Matrix3x3> matrix;
        EphemerisEngineQueryResult metadata;

        void recordMatrix(std::optional<skygate::core::Matrix3x3> value) noexcept;
        void mergeMetadataInto(EphemerisEngineQueryResult& target) const noexcept;
    };

    static constexpr std::size_t kMatrixKindCount = static_cast<std::size_t>(MatrixKind::PrecessionNutation) + 1U;

    [[nodiscard]] static constexpr EphemerisMetadataMergeOptions cachedMetadataMergeOptions() noexcept
    {
        return EphemerisMetadataMergeOptions::statusAndWarningsOnly();
    }

    void computeEntry(MatrixKind kind, Entry& entry);

    [[nodiscard]] std::optional<skygate::core::AstronomicalEpoch>
    epochInScale(skygate::core::TimeScale targetScale, EphemerisEngineQueryResult& metadata);
    [[nodiscard]] std::optional<EarthOrientationSampler::Sample> earthOrientation(EphemerisEngineQueryResult& metadata);
    [[nodiscard]] std::optional<skygate::core::AstronomicalEpoch> ut1Epoch(EphemerisEngineQueryResult& metadata);
    [[nodiscard]] std::optional<TimeScaleConversionResult>* conversionCacheFor(skygate::core::TimeScale targetScale);

    const skygate::core::AstronomicalEpoch m_epoch;
    const skygate::ephemeris::ITimeScaleService* const m_timeScaleService = nullptr;
    const skygate::ephemeris::IEarthOrientationProvider* const m_earthOrientationProvider = nullptr;
    std::optional<TimeScaleConversionResult> m_ttConversion;
    EphemerisMetadataMerger::EarthOrientationCache m_earthOrientationCache;
    std::optional<TimeScaleConversionResult> m_ut1Conversion;
    std::array<std::optional<Entry>, kMatrixKindCount> m_entries;
};

}  // namespace skygate::ephemeris::highprecision
