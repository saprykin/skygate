#pragma once

#include "engine/highprecision/HighPrecisionEphemerisEngine.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace skygate::ephemeris::highprecision {

enum class CelestialReferenceFrame : std::uint8_t {
    Icrs,
    Gcrs,
    TrueEquatorAndEquinox,
    Cirs,
    Tirs,
    Itrs
};

struct CelestialFrameVector {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct CelestialFrameTransformRequest {
    CelestialReferenceFrame sourceFrame = CelestialReferenceFrame::Gcrs;
    CelestialReferenceFrame targetFrame = CelestialReferenceFrame::Cirs;
    AstronomicalEpoch epoch;
    CelestialFrameVector vector;
};

struct CelestialFrameTransformStageMetadata {
    CelestialReferenceFrame sourceFrame = CelestialReferenceFrame::Gcrs;
    CelestialReferenceFrame targetFrame = CelestialReferenceFrame::Cirs;
    bool applied = false;
    EphemerisResultMetadata metadata;
};

struct CelestialFrameTransformResult {
    std::optional<CelestialFrameVector> vector;
    EphemerisResultMetadata metadata;
    std::vector<CelestialFrameTransformStageMetadata> stages;
};

class IFrameTransformer {
public:
    virtual ~IFrameTransformer() = default;

    [[nodiscard]] virtual CelestialFrameTransformResult
    transformCelestialVector(const CelestialFrameTransformRequest& request) const = 0;
};

class ErfaFrameTransformer final : public IFrameTransformer {
public:
    explicit ErfaFrameTransformer(
        std::shared_ptr<const skygate::ephemeris::ITimeScaleService> timeScaleService,
        std::shared_ptr<const skygate::ephemeris::IEarthOrientationProvider> earthOrientationProvider = nullptr
    );

    [[nodiscard]] CelestialFrameTransformResult transformCelestialVector(const CelestialFrameTransformRequest& request
    ) const override;

private:
    std::shared_ptr<const skygate::ephemeris::ITimeScaleService> m_timeScaleService;
    std::shared_ptr<const skygate::ephemeris::IEarthOrientationProvider> m_earthOrientationProvider;
};

}  // namespace skygate::ephemeris::highprecision
