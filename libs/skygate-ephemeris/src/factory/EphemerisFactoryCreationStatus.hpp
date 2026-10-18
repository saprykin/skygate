#pragma once

#include <cstdint>
#include <string_view>

namespace skygate::ephemeris {

class EphemerisFactoryCreationStatus {
public:
    enum class Type : std::uint8_t {
        CreatedRequestedEngine,
        CreatedSimpleFallback,
        FailedStrictHighPrecisionUnavailable,
        FailedInvalidRequest,
        FailedCreationError
    };

    EphemerisFactoryCreationStatus() = delete;

    // A creation status is a success whenever the factory produced an engine,
    // whether it is the requested one or the simple-engine fallback.
    [[nodiscard]] static constexpr bool isSuccess(const Type status) noexcept
    {
        return status == Type::CreatedRequestedEngine || status == Type::CreatedSimpleFallback;
    }

    [[nodiscard]] static constexpr std::string_view displayName(const Type status) noexcept
    {
        switch (status) {
        case Type::CreatedRequestedEngine:
            return "created requested engine";
        case Type::CreatedSimpleFallback:
            return "created simple engine fallback";
        case Type::FailedStrictHighPrecisionUnavailable:
            return "strict high precision unavailable";
        case Type::FailedInvalidRequest:
            return "invalid factory request";
        case Type::FailedCreationError:
            return "engine creation failed";
        }

        return {};
    }
};

}  // namespace skygate::ephemeris
