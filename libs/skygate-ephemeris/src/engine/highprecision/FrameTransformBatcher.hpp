#pragma once

#include "BaseCelestialBody.hpp"
#include "CelestialReferenceFrame.hpp"
#include "EphemerisRequest.hpp"
#include "HighPrecisionCalculatorResult.hpp"
#include "engine/EphemerisCorrectionFlags.hpp"
#include "engine/EphemerisEngineQueryResult.hpp"
#include "math/Vector3d.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace skygate::ephemeris {
class IEarthOrientationProvider;
class ITimeScaleService;
}  // namespace skygate::ephemeris

namespace skygate::ephemeris::highprecision {

struct PreparedEphemerisRequestState;

class IFrameTransformer;

// Collects the vectors a batch has to transform and runs the frame transform
// passes in the order the apparent place result depends on: topocentric
// preparation, the primary transform out of GCRS, then either ITRS to GCRS to
// apparent for a topocentric request or reuse of the primary output as the
// equatorial vector.
class FrameTransformBatcher final {
public:
    // One body of the batch with the vectors the passes read and write.
    struct Item {
        const BaseCelestialBody* body = nullptr;
        std::size_t bodyIndex = 0U;
        HighPrecisionCalculatorResult result;
        std::optional<skygate::core::Vector3d> outputVector;
        std::optional<skygate::core::Vector3d> equatorialVector;
    };

    FrameTransformBatcher(
        const IFrameTransformer* frameTransformer,
        const skygate::ephemeris::ITimeScaleService* timeScaleService,
        const skygate::ephemeris::IEarthOrientationProvider* earthOrientationProvider
    );

    void transform(
        const EphemerisRequest& request,
        const std::shared_ptr<const PreparedEphemerisRequestState>& preparedRequestState,
        std::span<Item> items
    ) const;

private:
    using ItemIndexList = std::vector<std::size_t>;
    using VectorList = std::vector<skygate::core::Vector3d>;

    enum class VectorDestination : std::uint8_t {
        Output,
        Equatorial
    };

    struct TransformRequest {
        CelestialReferenceFrame::Type sourceFrame = CelestialReferenceFrame::Type::Gcrs;
        CelestialReferenceFrame::Type targetFrame = CelestialReferenceFrame::Type::Gcrs;
        std::span<const skygate::core::Vector3d> inputs;
        std::span<const std::size_t> itemIndices;
        EphemerisCorrectionFlags unavailableCorrection;
        VectorDestination destination = VectorDestination::Output;
    };

    struct TopocentricRequestState {
        EphemerisEngineQueryResult metadata;
        bool available = true;
    };

    [[nodiscard]] static CelestialReferenceFrame::Type targetFrame(const EphemerisRequest& request) noexcept;

    [[nodiscard]] TopocentricRequestState prepareTopocentricRequestState(
        const EphemerisRequest& request, const PreparedEphemerisRequestState* requestState
    ) const;

    void collectPrimaryTransformInputs(
        CelestialReferenceFrame::Type requestedTargetFrame,
        const TopocentricRequestState& topocentricState,
        std::span<Item> items,
        VectorList& transformInputs,
        ItemIndexList& transformItemIndices
    ) const;

    // Gathers the vectors 'vectorMember' holds and runs one transform pass over
    // them, merging the transform metadata into the items the pass covered.
    void runVectorPass(
        const EphemerisRequest& request,
        std::span<Item> items,
        CelestialReferenceFrame::Type sourceFrame,
        CelestialReferenceFrame::Type targetFrame,
        const std::optional<skygate::core::Vector3d> Item::* vectorMember,
        EphemerisCorrectionFlags unavailableCorrection,
        VectorDestination destination
    ) const;

    void transformVectors(
        const EphemerisRequest& request, std::span<Item> items, const TransformRequest& transformRequest
    ) const;

    static void setTransformedVector(Item& item, VectorDestination destination, const skygate::core::Vector3d& vector);
    static void clearFailedTransformState(Item& item, VectorDestination destination);

    static void useOutputVectorsAsEquatorialVectors(std::span<Item> items);

    void applyTopocentricCorrections(
        const EphemerisRequest& request, const PreparedEphemerisRequestState* requestState, std::span<Item> items
    ) const;

    static void applyObserverParallax(Item& item, const std::optional<skygate::core::Vector3d>& observerPosition);
    static void updateHorizontalCoordinate(Item& item, const EphemerisRequest& request);

    void transformItrsVectorsToGcrs(const EphemerisRequest& request, std::span<Item> items) const;
    void transformGcrsVectorsToApparentWhenRequested(const EphemerisRequest& request, std::span<Item> items) const;

    const IFrameTransformer* const m_frameTransformer;
    const skygate::ephemeris::ITimeScaleService* const m_timeScaleService;
    const skygate::ephemeris::IEarthOrientationProvider* const m_earthOrientationProvider;
};

}  // namespace skygate::ephemeris::highprecision
