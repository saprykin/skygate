#pragma once

#include "HighPrecisionComputationInput.hpp"
#include "ICalcephKernel.hpp"
#include "engine/EphemerisCorrectionFlags.hpp"
#include "engine/EphemerisEngineQueryResult.hpp"
#include "math/Vector3d.hpp"

#include <optional>

namespace skygate::ephemeris::highprecision {

// Applies the requested correction stages to the geometric solar-system state
// and keeps the metadata in step with them.
class SolarSystemCorrectionPipeline final {
public:
    // A kernel state together with the target ids that produced it, so a
    // planetary-system barycenter fallback can be reported against the id that
    // was asked for.
    struct TargetKernelState {
        ICalcephKernel::StateResult state;
        int targetNaifId = 0;
        int requestedTargetNaifId = 0;
    };

    SolarSystemCorrectionPipeline(
        const ICalcephKernel& kernel,
        const HighPrecisionComputationInput& input,
        EphemerisEngineQueryResult& metadata,
        skygate::core::Vector3d outputVector,
        int effectiveTargetNaifId,
        std::optional<int> fallbackTargetNaifId,
        bool preferPlanetarySystemBarycenter
    );

    // Each enabled stage either replaces the output vector and marks the
    // correction applied, or marks it unavailable and leaves the vector alone.
    void applyCorrections(EphemerisCorrectionFlags requestedFlags);

    [[nodiscard]] skygate::core::Vector3d observerRelativePositionAu() const noexcept;

    [[nodiscard]] static TargetKernelState computeTargetKernelState(
        const ICalcephKernel& kernel,
        const skygate::core::AstronomicalEpoch& epoch,
        int targetNaifId,
        std::optional<int> fallbackTargetNaifId,
        int centerNaifId,
        bool preferFallbackTarget
    );

    static void markBarycenterFallback(
        EphemerisEngineQueryResult& metadata,
        const BaseCelestialBody& body,
        int requestedTargetNaifId,
        int effectiveTargetNaifId
    );

private:
    // Earth barycentric state, computed at most once and only when a stage
    // needs it.
    [[nodiscard]] const ICalcephKernel::StateResult& observerState();

    // Merges the stage dependency and reports whether the stage can run. An
    // unavailable dependency only contributes its warnings, never its
    // corrections.
    [[nodiscard]] bool beginStage(
        EphemerisCorrectionFlags correction,
        const ICalcephKernel::StateResult& dependencyState,
        bool dependencyAvailable
    );
    void finishStage(EphemerisCorrectionFlags correction, std::optional<skygate::core::Vector3d> correctedVector);

    void applyLightTime();
    void applyGravitationalLightDeflection();
    void applyStellarAberration();

    const ICalcephKernel& m_kernel;
    const HighPrecisionComputationInput& m_input;
    EphemerisEngineQueryResult& m_metadata;
    skygate::core::Vector3d m_outputVector;
    int m_effectiveTargetNaifId = 0;
    std::optional<int> m_fallbackTargetNaifId;
    bool m_preferPlanetarySystemBarycenter = false;
    std::optional<ICalcephKernel::StateResult> m_earthBarycentricState;
};

}  // namespace skygate::ephemeris::highprecision
