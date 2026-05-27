#pragma once

#include "engine/highprecision/HighPrecisionTypes.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
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

struct CelestialFrameBatchTransformRequest {
    CelestialReferenceFrame sourceFrame = CelestialReferenceFrame::Gcrs;
    CelestialReferenceFrame targetFrame = CelestialReferenceFrame::Cirs;
    AstronomicalEpoch epoch;
    std::span<const CelestialFrameVector> vectors;
};

struct CelestialFrameTransformStageMetadata {
    CelestialReferenceFrame sourceFrame = CelestialReferenceFrame::Gcrs;
    CelestialReferenceFrame targetFrame = CelestialReferenceFrame::Cirs;
    bool applied = false;
    EphemerisEngineQueryResult metadata;
};

struct CelestialFrameTransformResult {
    std::optional<CelestialFrameVector> vector;
    EphemerisEngineQueryResult metadata;
    std::vector<CelestialFrameTransformStageMetadata> stages;
};

class IFrameTransformer {
public:
    virtual ~IFrameTransformer() = default;

    [[nodiscard]] virtual CelestialFrameTransformResult
    transformCelestialVector(const CelestialFrameTransformRequest& request) const = 0;
    [[nodiscard]] virtual std::vector<CelestialFrameTransformResult>
    transformCelestialVectors(const CelestialFrameBatchTransformRequest& request) const
    {
        std::vector<CelestialFrameTransformResult> results;
        results.reserve(request.vectors.size());
        for (const CelestialFrameVector& vector : request.vectors) {
            results.push_back(transformCelestialVector(
                CelestialFrameTransformRequest{
                    .sourceFrame = request.sourceFrame,
                    .targetFrame = request.targetFrame,
                    .epoch = request.epoch,
                    .vector = vector,
                }
            ));
        }
        return results;
    }
};

class ErfaFrameTransformer final : public IFrameTransformer {
public:
    explicit ErfaFrameTransformer(
        std::shared_ptr<const skygate::ephemeris::ITimeScaleService> timeScaleService,
        std::shared_ptr<const skygate::ephemeris::IEarthOrientationProvider> earthOrientationProvider = nullptr
    );

    [[nodiscard]] CelestialFrameTransformResult
    transformCelestialVector(const CelestialFrameTransformRequest& request) const override;
    [[nodiscard]] std::vector<CelestialFrameTransformResult>
    transformCelestialVectors(const CelestialFrameBatchTransformRequest& request) const override;

private:
    std::shared_ptr<const skygate::ephemeris::ITimeScaleService> m_timeScaleService;
    std::shared_ptr<const skygate::ephemeris::IEarthOrientationProvider> m_earthOrientationProvider;
};

}  // namespace skygate::ephemeris::highprecision
