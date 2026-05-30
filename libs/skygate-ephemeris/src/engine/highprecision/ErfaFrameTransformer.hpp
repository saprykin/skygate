#pragma once

#include "IFrameTransformer.hpp"

#include <memory>

namespace skygate::ephemeris {
class IEarthOrientationProvider;
class ITimeScaleService;
}  // namespace skygate::ephemeris

namespace skygate::ephemeris::highprecision {

class ErfaFrameTransformer final : public IFrameTransformer {
public:
    explicit ErfaFrameTransformer(
        std::shared_ptr<const skygate::ephemeris::ITimeScaleService> timeScaleService,
        std::shared_ptr<const skygate::ephemeris::IEarthOrientationProvider> earthOrientationProvider = nullptr
    );

    [[nodiscard]] std::vector<CelestialFrameTransformResult>
    transform(const CelestialFrameTransformRequest& request) const override;

private:
    std::shared_ptr<const skygate::ephemeris::ITimeScaleService> m_timeScaleService;
    std::shared_ptr<const skygate::ephemeris::IEarthOrientationProvider> m_earthOrientationProvider;
};

}  // namespace skygate::ephemeris::highprecision
