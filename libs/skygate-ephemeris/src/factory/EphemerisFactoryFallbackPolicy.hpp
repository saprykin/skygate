#pragma once

#include <cstdint>

namespace skygate::ephemeris {

class EphemerisFactoryFallbackPolicy {
public:
    enum class Type : std::uint8_t {
        StrictHighPrecision,
        AllowSimpleEngineFallback
    };

    EphemerisFactoryFallbackPolicy() = delete;

    [[nodiscard]] static constexpr bool allowsSimpleEngineFallback(const Type policy) noexcept
    {
        return policy == Type::AllowSimpleEngineFallback;
    }
};

}  // namespace skygate::ephemeris
