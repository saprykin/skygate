#pragma once

#include <cstdint>
#include <string_view>

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

    [[nodiscard]] static constexpr std::string_view displayName(const Type policy) noexcept
    {
        switch (policy) {
        case Type::StrictHighPrecision:
            return "strict high precision";
        case Type::AllowSimpleEngineFallback:
            return "allow simple engine fallback";
        }

        return {};
    }
};

}  // namespace skygate::ephemeris
