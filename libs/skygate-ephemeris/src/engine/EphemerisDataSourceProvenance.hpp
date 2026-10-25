#pragma once

#include <cstdint>
#include <string_view>

namespace skygate::ephemeris {

class EphemerisDataSourceProvenance {
public:
    enum class Type : std::uint8_t {
        SimpleSolarSystemFallback
    };

    EphemerisDataSourceProvenance() = delete;

    [[nodiscard]] static constexpr std::string_view displayName(const Type type) noexcept
    {
        switch (type) {
        case Type::SimpleSolarSystemFallback:
            return "simple solar-system fallback";
        }

        return {};
    }
};

}  // namespace skygate::ephemeris
