#include "CelestialReferenceFrame.hpp"

namespace skygate::ephemeris::highprecision {

std::uint8_t CelestialReferenceFrame::rankFromType(const Type type) noexcept
{
    switch (type) {
    case Type::Icrs:
    case Type::Gcrs:
        return 0U;
    case Type::TrueEquatorAndEquinox:
    case Type::Cirs:
        return 1U;
    case Type::Tirs:
        return 2U;
    case Type::Itrs:
        return 3U;
    }

    return 0U;
}

CelestialReferenceFrame::Type CelestialReferenceFrame::typeFromRank(const std::uint8_t rank) noexcept
{
    switch (rank) {
    case 0U:
        return Type::Gcrs;
    case 1U:
        return Type::Cirs;
    case 2U:
        return Type::Tirs;
    case 3U:
        return Type::Itrs;
    default:
        return Type::Gcrs;
    }
}

bool CelestialReferenceFrame::isGcrsLike(const Type type) noexcept
{
    return type == Type::Icrs || type == Type::Gcrs;
}

bool CelestialReferenceFrame::isTerrestrial(const Type type) noexcept
{
    return type == Type::Tirs || type == Type::Itrs;
}

}  // namespace skygate::ephemeris::highprecision
