#pragma once

#include "engine/IEphemerisEngine.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <string_view>

namespace skygate::ephemeris::tests {

class MarkerGuidanceEngine final : public IEphemerisEngine {
public:
    explicit MarkerGuidanceEngine(std::shared_ptr<int> sampleCount);

    [[nodiscard]] EphemerisEngineKind::Type kind() const noexcept override;
    [[nodiscard]] std::string_view name() const noexcept override;

    [[nodiscard]] EphemerisSnapshot compute(const EphemerisRequest& request) const override;

    [[nodiscard]] std::optional<CelestialBodyState>
    computeBodyState(const EphemerisRequest& request, std::string_view bodyId) const override;

    [[nodiscard]] std::optional<CelestialBodyState>
    computeBodyState(const EphemerisRequest& request, std::size_t bodyIndex) const override;

    [[nodiscard]] EphemerisSnapshot compute(const skygate::core::ObservationContext& context) const override;

    [[nodiscard]] std::optional<CelestialBodyState>
    computeBodyState(const skygate::core::ObservationContext& context, std::string_view bodyId) const override;

    [[nodiscard]] std::optional<CelestialBodyState>
    computeBodyState(const skygate::core::ObservationContext& context, std::size_t bodyIndex) const override;

private:
    std::shared_ptr<int> m_sampleCount;
};

}  // namespace skygate::ephemeris::tests
