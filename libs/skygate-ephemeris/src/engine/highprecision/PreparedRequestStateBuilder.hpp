#pragma once

#include "EphemerisRequest.hpp"

#include <memory>

namespace skygate::ephemeris {
class ITimeScaleService;
class IEarthOrientationProvider;
}  // namespace skygate::ephemeris

namespace skygate::ephemeris::highprecision {

class ICalcephKernel;
struct PreparedEphemerisRequestState;

class PreparedRequestStateBuilder final {
public:
    struct Dependencies {
        std::shared_ptr<const ICalcephKernel> calcephKernel;
        std::shared_ptr<const skygate::ephemeris::ITimeScaleService> timeScaleService;
        std::shared_ptr<const skygate::ephemeris::IEarthOrientationProvider> earthOrientationProvider;
    };

    explicit PreparedRequestStateBuilder(Dependencies dependencies);

    [[nodiscard]] std::shared_ptr<const PreparedEphemerisRequestState> build(const EphemerisRequest& request) const;

private:
    Dependencies m_dependencies;
};

}  // namespace skygate::ephemeris::highprecision
