#pragma once

#include <cstdint>
#include <string_view>

namespace skygate::ephemeris {

class EphemerisEngineQueryStatus {
public:
    enum class Type : std::uint8_t {
        Valid = 0,
        Degraded = 1,
        Unsupported = 2,
        OutOfRange = 3,
        Failed = 4,
        Last = 5
    };

    EphemerisEngineQueryStatus() = delete;

    [[nodiscard]] static constexpr std::string_view displayName(Type status) noexcept
    {
        switch (status) {
        case Type::Valid:
            return "valid";
        case Type::Degraded:
            return "degraded";
        case Type::Unsupported:
            return "unsupported";
        case Type::OutOfRange:
            return "out of range";
        case Type::Failed:
            return "failed";
        case Type::Last:
            break;
        }

        return {};
    }
};

}  // namespace skygate::ephemeris
