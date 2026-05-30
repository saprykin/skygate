#pragma once

#include <cstdint>

namespace skygate::ephemeris::highprecision {

class CelestialReferenceFrame {
public:
    enum class Type : std::uint8_t {
        Icrs,
        Gcrs,
        TrueEquatorAndEquinox,
        Cirs,
        Tirs,
        Itrs
    };

    [[nodiscard]] static std::uint8_t rankFromType(Type type) noexcept;
    [[nodiscard]] static Type typeFromRank(std::uint8_t rank) noexcept;
    [[nodiscard]] static bool isGcrsLike(Type type) noexcept;
    [[nodiscard]] static bool isTerrestrial(Type type) noexcept;
};

}  // namespace skygate::ephemeris::highprecision
