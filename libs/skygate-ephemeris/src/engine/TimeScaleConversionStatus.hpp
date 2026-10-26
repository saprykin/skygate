#pragma once

#include <cstdint>
#include <string_view>

namespace skygate::ephemeris {

class TimeScaleConversionStatus {
public:
    enum class Type : std::uint8_t {
        Valid,
        Degraded,
        Failed
    };

    TimeScaleConversionStatus() = delete;

    [[nodiscard]] static constexpr std::string_view displayName(const Type status) noexcept
    {
        switch (status) {
        case Type::Valid:
            return "valid";
        case Type::Degraded:
            return "degraded";
        case Type::Failed:
            return "failed";
        }

        return {};
    }
};

}  // namespace skygate::ephemeris
