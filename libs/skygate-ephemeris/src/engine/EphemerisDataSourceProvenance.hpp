#pragma once

#include <cstdint>
#include <string_view>

namespace skygate::ephemeris {

class EphemerisDataSourceProvenance {
public:
    enum class Type : std::uint8_t {
        Unknown = 0,
        SimpleEngine = 1,
        SimpleSolarSystemFallback = 2
    };

    EphemerisDataSourceProvenance() = delete;

    [[nodiscard]] static constexpr std::string_view displayName(const Type type) noexcept
    {
        switch (type) {
        case Type::Unknown:
            return {};
        case Type::SimpleEngine:
            return "simple engine";
        case Type::SimpleSolarSystemFallback:
            return "simple solar-system fallback";
        }

        return {};
    }
};

}  // namespace skygate::ephemeris
