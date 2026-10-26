#include "AngleMath.hpp"
#include "MathConstants.hpp"

#include <cmath>

namespace skygate::core {

double AngleMath::toRadians(const double degrees) noexcept
{
    return degrees * MathConstants::kDegreesToRadians;
}

double AngleMath::toDegrees(const double radians) noexcept
{
    return radians * MathConstants::kRadiansToDegrees;
}

double AngleMath::hoursToRadians(const double hours) noexcept
{
    return hours * MathConstants::kRadiansPerHour;
}

double AngleMath::radiansToHours(const double radians) noexcept
{
    return radians * MathConstants::kHoursPerRadian;
}

double AngleMath::hoursToDegrees(const double hours) noexcept
{
    return hours * MathConstants::kDegreesPerHour;
}

double AngleMath::degreesToHours(const double degrees) noexcept
{
    return degrees / MathConstants::kDegreesPerHour;
}

double AngleMath::normalizeDegrees(double degrees) noexcept
{
    degrees = std::fmod(degrees, MathConstants::kDegreesPerTurn);
    if (degrees < 0.0) {
        degrees += MathConstants::kDegreesPerTurn;
    }
    return degrees;
}

double AngleMath::normalizeDegreesSigned(double degrees) noexcept
{
    double normalized = normalizeDegrees(degrees);
    if (normalized > MathConstants::kDegreesPerTurn / 2.0) {
        normalized -= MathConstants::kDegreesPerTurn;
    }
    return normalized;
}

double AngleMath::normalizeHours(double hours) noexcept
{
    hours = std::fmod(hours, MathConstants::kHoursPerTurn);
    if (hours < 0.0) {
        hours += MathConstants::kHoursPerTurn;
    }
    return hours;
}

}  // namespace skygate::core
