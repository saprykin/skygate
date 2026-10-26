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

    /// Canonical topocentric preparation: the observer ITRS position, the
    /// Earth-orientation sample for the request epoch, and the metadata both
    /// produce.
    ///
    /// Fills the topocentric part of 'state' and sets its availability. A
    /// missing time-scale service leaves the state unavailable and marks the
    /// Earth-orientation correction unavailable.
    static void prepareTopocentricState(
        const EphemerisRequest& request,
        const skygate::ephemeris::ITimeScaleService* timeScaleService,
        const skygate::ephemeris::IEarthOrientationProvider* earthOrientationProvider,
        PreparedEphemerisRequestState& state
    );

private:
    Dependencies m_dependencies;
};

}  // namespace skygate::ephemeris::highprecision
