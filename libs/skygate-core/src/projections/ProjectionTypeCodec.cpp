#include "ProjectionTypeCodec.hpp"

#include <array>
#include <cstddef>
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

[[nodiscard]] constexpr bool isAsciiWhitespace(const char value) noexcept
{
    return value == ' ' || value == '\t' || value == '\n' || value == '\r' || value == '\f' || value == '\v';
}

[[nodiscard]] constexpr char toAsciiLower(const char value) noexcept
{
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value;
}

[[nodiscard]] constexpr std::string_view trimAsciiWhitespace(const std::string_view text) noexcept
{
    std::size_t begin = 0;
    while (begin < text.size() && isAsciiWhitespace(text[begin])) {
        ++begin;
    }

    std::size_t end = text.size();
    while (end > begin && isAsciiWhitespace(text[end - 1])) {
        --end;
    }
    return text.substr(begin, end - begin);
}

[[nodiscard]] constexpr bool equalsAsciiIgnoreCase(const std::string_view lhs, const std::string_view rhs) noexcept
{
    if (lhs.size() != rhs.size()) {
        return false;
    }

    for (std::size_t index = 0; index < lhs.size(); ++index) {
        if (toAsciiLower(lhs[index]) != toAsciiLower(rhs[index])) {
            return false;
        }
    }
    return true;
}

}  // namespace

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
    const std::string_view trimmed = trimAsciiWhitespace(text);
    for (const ProjectionType value : kOrderedValues) {
        if (equalsAsciiIgnoreCase(trimmed, ProjectionTypeCodec::text(value))) {
            return value;
        }
    }
    return std::nullopt;
}

}  // namespace skygate::core
