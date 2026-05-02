#pragma once

#include "skygate/core/Types.hpp"
#include "skygate/ephemeris/Types.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace skygate::ephemeris {

namespace detail {

[[nodiscard]] constexpr char lowerAscii(const char character) noexcept
{
    return character >= 'A' && character <= 'Z'
        ? static_cast<char>(character - 'A' + 'a')
        : character;
}

[[nodiscard]] constexpr bool bodyIdEquals(
    const std::string_view lhs,
    const std::string_view rhs
) noexcept
{
    if (lhs.size() != rhs.size()) {
        return false;
    }

    for (std::size_t index = 0; index < lhs.size(); ++index) {
        if (lowerAscii(lhs[index]) != lowerAscii(rhs[index])) {
            return false;
        }
    }

    return true;
}

}  // namespace detail

class IEphemerisEngine {
public:
    virtual ~IEphemerisEngine() = default;

    [[nodiscard]] virtual SkySnapshot compute(const core::SkyContext& context) const = 0;
    [[nodiscard]] virtual std::optional<CelestialBodyState> computeBodyState(
        const core::SkyContext& context,
        std::string_view bodyId
    ) const
    {
        const SkySnapshot snapshot = compute(context);
        for (const CelestialBodyState& state : snapshot.states) {
            const CelestialBody& body = snapshot.bodyAt(state.bodyIndex);
            if (detail::bodyIdEquals(body.id, bodyId)) {
                return state;
            }
        }

        return std::nullopt;
    }

    [[nodiscard]] virtual std::optional<CelestialBodyState> computeBodyState(
        const core::SkyContext& context,
        std::uint32_t bodyIndex
    ) const
    {
        const SkySnapshot snapshot = compute(context);
        for (const CelestialBodyState& state : snapshot.states) {
            if (state.bodyIndex == bodyIndex) {
                return state;
            }
        }

        return std::nullopt;
    }
};

}  // namespace skygate::ephemeris
