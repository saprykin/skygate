#include "SkyRenderLabels.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <utility>

namespace {

// Above this field width a deep-sky label needs a common name; catalog-only
// names are too dense to read.
constexpr double kCommonNameRequiredAboveFovDeg = 20.0;
// No deep-sky labels above this field width. At or below the close-up width
// every candidate is allowed.
constexpr double kWideFieldLabelCutoffFovDeg = 60.0;
constexpr double kCloseUpLabelFovDeg = 2.0;
constexpr std::size_t kCloseUpLabelBudget = 1000U;
constexpr std::size_t kNarrowFieldLabelBudget = 150U;

struct DeepSkyLabelBudgetTier final {
    double minFovDeg;
    std::size_t baseBudget;
};

// Budget tiers for the remaining field widths: the first tier the field width
// exceeds wins. The narrowest tier always matches once the close-up case has
// returned.
constexpr DeepSkyLabelBudgetTier kDeepSkyLabelBudgetTiers[] = {
    {.minFovDeg = 35.0, .baseBudget = 18U},
    {.minFovDeg = 20.0, .baseBudget = 32U},
    {.minFovDeg = 10.0, .baseBudget = 60U},
    {.minFovDeg = 5.0, .baseBudget = 100U},
    {.minFovDeg = 2.0, .baseBudget = kNarrowFieldLabelBudget},
};

// Label budgets scale with the viewport relative to a reference area tuned
// here, independently of the default window size.
constexpr double kLabelScaleReferenceWidthPx = 1100.0;
constexpr double kLabelScaleReferenceHeightPx = 760.0;
constexpr double kLabelViewportScaleMin = 0.70;
constexpr double kLabelViewportScaleMax = 1.35;

constexpr double kMessierLabelScore = 1000.0;
constexpr double kCommonNameLabelScore = 700.0;
// Magnitudes fainter than this add no brightness score.
constexpr double kFaintestScoredMagnitude = 14.0;
constexpr double kMagnitudeScoreMax = 16.0;
constexpr double kMagnitudeScoreWeight = 12.0;
constexpr double kMajorAxisScoreMaxArcmin = 120.0;
constexpr double kMajorAxisScoreWeightPerArcmin = 0.75;
constexpr double kCenterDistanceScoreWeight = 120.0;

struct DeepSkyLabelCandidate final {
    std::size_t glyphIndex = 0;
    double score = 0.0;
    skygate::core::Rect2d bounds;
};

[[nodiscard]] bool startsWithCatalogPrefix(const std::string_view text, const std::string_view prefix)
{
    return text.size() > prefix.size()
           && std::equal(
               prefix.begin(),
               prefix.end(),
               text.begin(),
               text.begin() + static_cast<std::ptrdiff_t>(prefix.size()),
               [](const char lhs, const char rhs) {
                   return std::tolower(static_cast<unsigned char>(lhs))
                          == std::tolower(static_cast<unsigned char>(rhs));
               }
           )
           && text.at(prefix.size()) == ' ';
}

[[nodiscard]] bool isMessierObject(const skygate::ephemeris::BaseCelestialBody& body)
{
    if (body.id.starts_with("messier_")) {
        return true;
    }

    if (body.displayName.size() < 2U || body.displayName.front() != 'M') {
        return false;
    }

    return std::all_of(body.displayName.begin() + 1, body.displayName.end(), [](const char value) {
        return std::isdigit(static_cast<unsigned char>(value));
    });
}

[[nodiscard]] bool isGenericCatalogAlias(const std::string_view alias)
{
    return startsWithCatalogPrefix(alias, "M") || startsWithCatalogPrefix(alias, "NGC")
           || startsWithCatalogPrefix(alias, "IC") || startsWithCatalogPrefix(alias, "PGC")
           || startsWithCatalogPrefix(alias, "UGC") || startsWithCatalogPrefix(alias, "ESO")
           || startsWithCatalogPrefix(alias, "MCG") || startsWithCatalogPrefix(alias, "CGCG")
           || startsWithCatalogPrefix(alias, "2MASX") || startsWithCatalogPrefix(alias, "IRAS")
           || startsWithCatalogPrefix(alias, "PK");
}

[[nodiscard]] bool hasCommonNameAlias(const skygate::ephemeris::BaseCelestialBody& body)
{
    if (!body.deepSkyObjectValue().has_value()) {
        return false;
    }

    return std::any_of(
        body.deepSkyObjectValue()->aliases.begin(),
        body.deepSkyObjectValue()->aliases.end(),
        [](const std::string& alias) { return !alias.empty() && !isGenericCatalogAlias(alias); }
    );
}

[[nodiscard]] bool shouldConsiderDeepSkyLabel(const skygate::ephemeris::BaseCelestialBody& body, const double fovDeg)
{
    if (isMessierObject(body) || hasCommonNameAlias(body)) {
        return true;
    }

    return fovDeg <= kCommonNameRequiredAboveFovDeg;
}

[[nodiscard]] std::size_t
deepSkyLabelBudget(const double fovDeg, const double viewportWidth, const double viewportHeight)
{
    if (fovDeg > kWideFieldLabelCutoffFovDeg) {
        return 0U;
    }
    if (fovDeg <= kCloseUpLabelFovDeg) {
        return kCloseUpLabelBudget;
    }

    std::size_t baseBudget = kNarrowFieldLabelBudget;
    for (const DeepSkyLabelBudgetTier& tier : kDeepSkyLabelBudgetTiers) {
        if (fovDeg > tier.minFovDeg) {
            baseBudget = tier.baseBudget;
            break;
        }
    }

    const double viewportScale = std::clamp(
        skygate::core::Geometry2d::areaScale(
            viewportWidth, viewportHeight, kLabelScaleReferenceWidthPx, kLabelScaleReferenceHeightPx
        ),
        kLabelViewportScaleMin,
        kLabelViewportScaleMax
    );
    return static_cast<std::size_t>(std::max(1.0, static_cast<double>(baseBudget) * viewportScale));
}

[[nodiscard]] double deepSkyLabelScore(
    const skygate::ephemeris::BaseCelestialBody& body,
    const SkyRenderGlyph& glyph,
    const double viewportWidth,
    const double viewportHeight
)
{
    double score = 0.0;
    if (isMessierObject(body)) {
        score += kMessierLabelScore;
    } else if (hasCommonNameAlias(body)) {
        score += kCommonNameLabelScore;
    }

    if (std::isfinite(body.visualMagnitude)) {
        score += std::clamp(kFaintestScoredMagnitude - body.visualMagnitude, 0.0, kMagnitudeScoreMax)
                 * kMagnitudeScoreWeight;
    }

    if (body.deepSkyObjectValue().has_value()) {
        score += std::clamp(body.deepSkyObjectValue()->majorAxisArcmin.value_or(0.0), 0.0, kMajorAxisScoreMaxArcmin)
                 * kMajorAxisScoreWeightPerArcmin;
    }

    const double centerX = viewportWidth * 0.5;
    const double centerY = viewportHeight * 0.5;
    const double normalizedDistanceSquared =
        skygate::core::Geometry2d::squaredDistance2d(glyph.x, glyph.y, centerX, centerY)
        / std::max(1.0, viewportWidth * viewportWidth + viewportHeight * viewportHeight);

    return score - (normalizedDistanceSquared * kCenterDistanceScoreWeight);
}

}  // namespace

namespace skygate::ui::internal {

QColor skyRenderLabelColorForBodyType(
    const skygate::ephemeris::BaseCelestialBody::Kind type, const SkyThemeRenderPalette& renderTheme
)
{
    switch (type) {
    case skygate::ephemeris::BaseCelestialBody::Kind::Sun:
        return renderTheme.labelSun;
    case skygate::ephemeris::BaseCelestialBody::Kind::Moon:
        return renderTheme.labelMoon;
    case skygate::ephemeris::BaseCelestialBody::Kind::Planet:
        return renderTheme.labelPlanet;
    case skygate::ephemeris::BaseCelestialBody::Kind::Constellation:
        return renderTheme.labelConstellation;
    case skygate::ephemeris::BaseCelestialBody::Kind::DeepSkyObject:
        return renderTheme.labelDeepSkyObject;
    case skygate::ephemeris::BaseCelestialBody::Kind::Star:
        break;
    }

    return renderTheme.labelDefault;
}

skygate::core::Rect2d skyRenderLabelBounds(const double anchorX, const double anchorY, const std::string_view text)
{
    constexpr double kLabelPaddingX = 12.0;
    constexpr double kLabelHeightPx = 20.0;
    constexpr double kLabelOffsetYPx = 8.0;
    constexpr double kApproximateGlyphWidthPx = 6.8;

    const double width = std::max(24.0, (static_cast<double>(text.size()) * kApproximateGlyphWidthPx) + kLabelPaddingX);
    const double left = anchorX - (width * 0.5);
    const double top = anchorY - kLabelHeightPx - kLabelOffsetYPx;
    return skygate::core::Rect2d{.left = left, .top = top, .right = left + width, .bottom = top + kLabelHeightPx};
}

bool skyRenderLabelFitsViewport(
    const skygate::core::Rect2d& bounds,
    const double viewportWidth,
    const double viewportHeight,
    const double edgeMarginPx
)
{
    return skygate::core::Geometry2d::fitsWithin(bounds, viewportWidth, viewportHeight, edgeMarginPx);
}

void appendSkyRenderLabel(
    std::vector<SkyRenderLabel>& labels, QString kind, const double x, const double y, QString text, const QColor& color
)
{
    labels.push_back(SkyRenderLabel{.kind = std::move(kind), .x = x, .y = y, .text = std::move(text), .color = color});
}

void appendSkyRenderLabel(
    std::vector<SkyRenderLabel>& labels,
    const double x,
    const double y,
    const std::string_view text,
    const QColor& color
)
{
    appendSkyRenderLabel(labels, {}, x, y, QString::fromStdString(std::string(text)), color);
}

double skyRenderDeepSkyHitRadius(const SkyRenderGlyph& glyph)
{
    return std::max({glyph.radiusXPx, glyph.radiusYPx, 8.0});
}

void appendBodyPointLabels(
    SkyRenderFrame& frame,
    const skygate::ephemeris::EphemerisSnapshot& snapshot,
    const double viewportWidth,
    const double viewportHeight,
    const SkyThemeRenderPalette& renderTheme,
    const SkyOverlayLayerVisibility& overlayLayers,
    const double edgeMarginPx,
    std::unordered_set<std::string_view>& seenLabels,
    skygate::core::RectOccupancyGrid& labelGrid
)
{
    for (const auto& point : frame.points) {
        const auto& body = snapshot.bodyAt(point.bodyIndex);
        const bool isConstellationLabel = body.kind == skygate::ephemeris::BaseCelestialBody::Kind::Constellation;
        const bool isSolarSystemLabel = body.kind == skygate::ephemeris::BaseCelestialBody::Kind::Planet
                                        || body.kind == skygate::ephemeris::BaseCelestialBody::Kind::Sun
                                        || body.kind == skygate::ephemeris::BaseCelestialBody::Kind::Moon;
        if ((!isConstellationLabel || !overlayLayers.constellationLabels)
            && (!isSolarSystemLabel || !overlayLayers.solarSystemLabels)) {
            continue;
        }

        if (body.displayName.empty() || !seenLabels.insert(body.displayName).second) {
            continue;
        }

        if (point.x < edgeMarginPx || point.x > (viewportWidth - edgeMarginPx) || point.y < edgeMarginPx
            || point.y > (viewportHeight - edgeMarginPx)) {
            continue;
        }

        const skygate::core::Rect2d bounds = skyRenderLabelBounds(point.x, point.y, body.displayName);
        appendSkyRenderLabel(
            frame.labels, point.x, point.y, body.displayName, skyRenderLabelColorForBodyType(body.kind, renderTheme)
        );
        labelGrid.add(bounds);
    }
}

void appendDeepSkyLabels(
    SkyRenderFrame& frame,
    const skygate::ephemeris::EphemerisSnapshot& snapshot,
    const skygate::core::ProjectionParams& projectionParams,
    const double viewportWidth,
    const double viewportHeight,
    const SkyThemeRenderPalette& renderTheme,
    const double edgeMarginPx,
    std::unordered_set<std::string_view>& seenLabels,
    skygate::core::RectOccupancyGrid& labelGrid
)
{
    const std::size_t labelBudget = deepSkyLabelBudget(projectionParams.fovDeg, viewportWidth, viewportHeight);
    if (labelBudget == 0U) {
        return;
    }

    std::vector<DeepSkyLabelCandidate> candidates;
    candidates.reserve(std::min(frame.glyphs.size(), labelBudget * 4U));
    for (std::size_t glyphIndex = 0; glyphIndex < frame.glyphs.size(); ++glyphIndex) {
        const auto& glyph = frame.glyphs[glyphIndex];
        const auto& body = snapshot.bodyAt(glyph.bodyIndex);
        if (body.displayName.empty() || seenLabels.contains(body.displayName)
            || !shouldConsiderDeepSkyLabel(body, projectionParams.fovDeg)) {
            continue;
        }

        const double anchorY = glyph.y - skyRenderDeepSkyHitRadius(glyph);
        const skygate::core::Rect2d bounds = skyRenderLabelBounds(glyph.x, anchorY, body.displayName);
        if (!skyRenderLabelFitsViewport(bounds, viewportWidth, viewportHeight, edgeMarginPx)) {
            continue;
        }

        candidates.push_back(
            DeepSkyLabelCandidate{
                .glyphIndex = glyphIndex,
                .score = deepSkyLabelScore(body, glyph, viewportWidth, viewportHeight),
                .bounds = bounds
            }
        );
    }

    std::sort(
        candidates.begin(),
        candidates.end(),
        [&frame, &snapshot](const DeepSkyLabelCandidate& lhs, const DeepSkyLabelCandidate& rhs) {
            if (std::abs(lhs.score - rhs.score) > 1e-9) {
                return lhs.score > rhs.score;
            }

            const auto& lhsBody = snapshot.bodyAt(frame.glyphs[lhs.glyphIndex].bodyIndex);
            const auto& rhsBody = snapshot.bodyAt(frame.glyphs[rhs.glyphIndex].bodyIndex);
            return lhsBody.displayName < rhsBody.displayName;
        }
    );

    std::size_t acceptedCount = 0U;
    for (const DeepSkyLabelCandidate& candidate : candidates) {
        if (acceptedCount >= labelBudget) {
            break;
        }

        if (labelGrid.collides(candidate.bounds)) {
            continue;
        }

        const auto& glyph = frame.glyphs[candidate.glyphIndex];
        const auto& body = snapshot.bodyAt(glyph.bodyIndex);
        if (!seenLabels.insert(body.displayName).second) {
            continue;
        }

        labelGrid.add(candidate.bounds);
        appendSkyRenderLabel(
            frame.labels,
            glyph.x,
            glyph.y - skyRenderDeepSkyHitRadius(glyph),
            body.displayName,
            skyRenderLabelColorForBodyType(body.kind, renderTheme)
        );
        ++acceptedCount;
    }
}

}  // namespace skygate::ui::internal
