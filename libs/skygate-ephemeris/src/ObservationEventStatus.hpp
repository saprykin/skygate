#pragma once

#include <cstdint>
#include <string_view>

namespace skygate::ephemeris {

class ObservationEventStatus {
public:
    enum class Type : std::uint8_t {
        Available,
        AlwaysAbove,
        AlwaysBelow,
        NoEventInSearchWindow,
        InvalidInput,
        Unresolved
    };

    ObservationEventStatus() = delete;

    [[nodiscard]] static constexpr std::string_view displayText(const Type status) noexcept
    {
        switch (status) {
        case Type::Available:
            return "Available";
        case Type::AlwaysAbove:
            return "Always up";
        case Type::AlwaysBelow:
            return "Always down";
        case Type::NoEventInSearchWindow:
            return "No event";
        case Type::InvalidInput:
            return "Invalid input";
        case Type::Unresolved:
            return "Unresolved";
        }

        return {};
    }
};

}  // namespace skygate::ephemeris
