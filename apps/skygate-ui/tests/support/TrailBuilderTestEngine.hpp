#pragma once

#include "engine/IEphemerisEngine.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace skygate::ui::tests {

// Configurable ephemeris double for the trail-builder suites. It returns a
// linear altitude/azimuth ramp around the observation time, can drop the
// present sample, and records the request offsets it was asked for.
class TrailBuilderTestEngine final : public skygate::ephemeris::IEphemerisEngine {
public:
    explicit TrailBuilderTestEngine(std::uint32_t expectedBodyIndex = 7U);

    [[nodiscard]] skygate::ephemeris::EphemerisEngineKind::Type kind() const noexcept override;
    [[nodiscard]] std::string_view name() const noexcept override;
    [[nodiscard]] skygate::ephemeris::EphemerisEngineTraits traits() const noexcept override;

    void setHighPrecisionTraits(bool highPrecisionTraits) noexcept;
    void setTraits(skygate::ephemeris::EphemerisEngineTraits traits) noexcept;
    void setNonEnumeratedKind(bool nonEnumeratedKind) noexcept;
    void setGapAtPresent(bool gapAtPresent) noexcept;

    [[nodiscard]] skygate::ephemeris::EphemerisSnapshot
    compute(const skygate::ephemeris::EphemerisRequest& request) const override;

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::ephemeris::EphemerisRequest& request, std::string_view bodyId) const override;

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::ephemeris::EphemerisRequest& request, std::size_t bodyIndex) const override;

    [[nodiscard]] skygate::ephemeris::EphemerisSnapshot
    compute(const skygate::core::ObservationContext& context) const override;

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::ObservationContext& context, std::string_view bodyId) const override;

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::ObservationContext& context, std::size_t bodyIndex) const override;

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    stateForContext(const skygate::core::ObservationContext& context, std::uint32_t bodyIndex) const;

    [[nodiscard]] bool sawExpectedBodyIndex() const noexcept;
    [[nodiscard]] int requestBodyStateCalls() const noexcept;
    [[nodiscard]] int contextBodyStateCalls() const noexcept;
    [[nodiscard]] const std::optional<skygate::ephemeris::EphemerisRequest>& lastRequest() const noexcept;
    [[nodiscard]] const std::vector<int>& requestOffsetMinutes() const noexcept;

private:
    std::uint32_t m_expectedBodyIndex = 0;
    skygate::ephemeris::EphemerisEngineTraits m_traits = skygate::ephemeris::EphemerisEngineTraits::noTraits();
    bool m_nonEnumeratedKind = false;
    bool m_gapAtPresent = false;
    mutable bool m_sawExpectedBodyIndex = false;
    mutable int m_requestBodyStateCalls = 0;
    mutable int m_contextBodyStateCalls = 0;
    mutable std::optional<skygate::ephemeris::EphemerisRequest> m_lastRequest;
    mutable std::vector<int> m_requestOffsetMinutes;
};

}  // namespace skygate::ui::tests
