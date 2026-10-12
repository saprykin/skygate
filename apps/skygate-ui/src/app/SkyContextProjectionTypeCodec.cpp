#include "SkyContextProjectionTypeCodec.hpp"

namespace skygate::ui::internal {

QString SkyContextProjectionTypeCodec::toString(const skygate::core::ProjectionType projectionType)
{
    switch (projectionType) {
    case skygate::core::ProjectionType::Stereographic:
        return "Stereographic";
    case skygate::core::ProjectionType::AzimuthalEquidistant:
        return "AzimuthalEquidistant";
    case skygate::core::ProjectionType::Perspective:
        return "Perspective";
    }

    return "Unknown";
}

std::optional<skygate::core::ProjectionType> SkyContextProjectionTypeCodec::fromString(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == "stereographic") {
        return skygate::core::ProjectionType::Stereographic;
    }

    if (normalized == "azimuthalequidistant") {
        return skygate::core::ProjectionType::AzimuthalEquidistant;
    }

    if (normalized == "perspective") {
        return skygate::core::ProjectionType::Perspective;
    }

    return std::nullopt;
}

}  // namespace skygate::ui::internal
