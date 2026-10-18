#include "SkySelectionTargetKind.hpp"
#include "scene/SkySceneLookupKey.hpp"

namespace skygate::ui::internal {
namespace {

constexpr const char* kBodyToken = "body";
constexpr const char* kConstellationLabelToken = "constellationLabel";
// Comparison form of the constellation label token. Tokens are compared after
// trimming and lowercasing, so this also matches the older
// "constellationlabel" spelling.
constexpr const char* kConstellationLabelComparisonToken = "constellationlabel";

}  // namespace

QString SkySelectionTargetKind::token(const Type kind)
{
    switch (kind) {
    case Type::Body:
        return QString::fromLatin1(kBodyToken);
    case Type::ConstellationLabel:
        return QString::fromLatin1(kConstellationLabelToken);
    }

    return {};
}

std::optional<SkySelectionTargetKind::Type> SkySelectionTargetKind::fromToken(const QString& text)
{
    const QString normalizedToken = SkySceneLookupKey::normalized(text);
    if (normalizedToken == QLatin1String(kBodyToken)) {
        return Type::Body;
    }
    if (normalizedToken == QLatin1String(kConstellationLabelComparisonToken)) {
        return Type::ConstellationLabel;
    }

    return std::nullopt;
}

bool SkySelectionTargetKind::isBody(const QString& text)
{
    return fromToken(text) == Type::Body;
}

bool SkySelectionTargetKind::isConstellationLabel(const QString& text)
{
    return fromToken(text) == Type::ConstellationLabel;
}

}  // namespace skygate::ui::internal
