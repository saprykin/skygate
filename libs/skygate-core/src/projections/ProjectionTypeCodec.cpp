#include "ProjectionTypeCodec.hpp"
#include "AsciiText.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <string_view>
#include <vector>

namespace skygate::core {
namespace {

constexpr std::array<ProjectionType, 3> kOrderedValues{
    ProjectionType::Stereographic,
    ProjectionType::AzimuthalEquidistant,
    ProjectionType::Perspective,
};

}  // namespace

bool ProjectionTypeCodec::isKnown(const ProjectionType projectionType) noexcept
{
    return std::find(kOrderedValues.begin(), kOrderedValues.end(), projectionType) != kOrderedValues.end();
}

std::vector<ProjectionType> ProjectionTypeCodec::orderedValues()
{
    return std::vector<ProjectionType>(kOrderedValues.begin(), kOrderedValues.end());
}

std::string_view ProjectionTypeCodec::text(const ProjectionType projectionType) noexcept
{
    switch (projectionType) {
    case ProjectionType::Stereographic:
        return "Stereographic";
    case ProjectionType::AzimuthalEquidistant:
        return "AzimuthalEquidistant";
    case ProjectionType::Perspective:
        return "Perspective";
    }

    return {};
}

std::optional<ProjectionType> ProjectionTypeCodec::fromText(const std::string_view text) noexcept
{
    const std::string_view trimmed = skygate::core::AsciiText::trimWhitespace(text);
    for (const ProjectionType value : kOrderedValues) {
        if (skygate::core::AsciiText::equalsIgnoreCase(trimmed, ProjectionTypeCodec::text(value))) {
            return value;
        }
    }
    return std::nullopt;
}

}  // namespace skygate::core
