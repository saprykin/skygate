#pragma once

#include <cstdint>

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
};

}  // namespace skygate::ephemeris
