#pragma once

#include <cstdint>
#include <string_view>

namespace skygate::ephemeris {

class EphemerisEngineKind {
public:
    enum class Type : std::uint8_t {
        Simple = 0,
        HighPrecision = 1,
        Last = 2
    };

    EphemerisEngineKind() = delete;

    [[nodiscard]] static constexpr std::string_view displayName(Type kind) noexcept
    {
        switch (kind) {
        case Type::Simple:
            return "Simple";
        case Type::HighPrecision:
            return "High precision";
        case Type::Last:
            break;
        }

        return {};
    }
};

}  // namespace skygate::ephemeris
