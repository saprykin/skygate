#include "SkyRenderLabels.hpp"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <string>
#include <utility>

namespace {

struct DeepSkyLabelCandidate final {
    std::size_t glyphIndex = 0;
    double score = 0.0;
    skygate::core::Rect2d bounds;
};

[[nodiscard]] bool startsWithCatalogPrefix(
    const std::string_view text,
    const std::string_view prefix
)
{
    return text.size() > prefix.size()
        && std::equal(
            prefix.begin(),
            prefix.end(),
            text.begin(),
            text.begin() + static_cast<std::ptrdiff_t>(prefix.size()),
            [](const char lhs, const char rhs) {
                return std::tolower(
                    static_cast<unsigned char>(lhs)
                ) == std::tolower(static_cast<unsigned char>(rhs));
            }
        )
        && text.at(prefix.size()) == ' ';
}

[[nodiscard]] bool isMessierObject(const skygate::ephemeris::CelestialBody& body)
{
    if (body.id.starts_with("messier_")) {
        return true;
    }

    if (body.displayName.size() < 2U || body.displayName.front() != 'M') {
        return false;
    }

    return std::all_of(
        body.displayName.begin() + 1,
        body.displayName.end(),
        [](const char value) {
            return std::isdigit(static_cast<unsigned char>(value));
        }
    );
}

[[nodiscard]] bool isGenericCatalogAlias(const std::string_view alias)
{
    return startsWithCatalogPrefix(alias, "M")
        || startsWithCatalogPrefix(alias, "NGC")
        || startsWithCatalogPrefix(alias, "IC")
        || startsWithCatalogPrefix(alias, "PGC")
        || startsWithCatalogPrefix(alias, "UGC")
        || startsWithCatalogPrefix(alias, "ESO")
        || startsWithCatalogPrefix(alias, "MCG")
        || startsWithCatalogPrefix(alias, "CGCG")
        || startsWithCatalogPrefix(alias, "2MASX")
        || startsWithCatalogPrefix(alias, "IRAS")
        || startsWithCatalogPrefix(alias, "PK");
}

[[nodiscard]] bool hasCommonNameAlias(const skygate::ephemeris::CelestialBody& body)
{
    if (!body.deepSkyObject.has_value()) {
        return false;
    }

    return std::any_of(
        body.deepSkyObject->aliases.begin(),
        body.deepSkyObject->aliases.end(),
        [](const std::string& alias) {
            return !alias.empty() && !isGenericCatalogAlias(alias);
        }
    );
}

[[nodiscard]] bool shouldConsiderDeepSkyLabel(
    const skygate::ephemeris::CelestialBody& body,
    const double fovDeg
)
{
    if (isMessierObject(body) || hasCommonNameAlias(body)) {
        return true;
    }

    return fovDeg <= 20.0;
}

[[nodiscard]] std::size_t deepSkyLabelBudget(
    const double fovDeg,
    const double viewportWidth,
    const double viewportHeight
)
{
    if (fovDeg > 60.0) {
        return 0U;
    }
    if (fovDeg <= 2.0) {
        return 1000U;
    }

    std::size_t baseBudget = 150U;
    if (fovDeg > 35.0) {
        baseBudget = 18U;
    } else if (fovDeg > 20.0) {
        baseBudget = 32U;
    } else if (fovDeg > 10.0) {
        baseBudget = 60U;
    } else if (fovDeg > 5.0) {
        baseBudget = 100U;
    }

    const double viewportScale = std::clamp(
        skygate::core::areaScale(viewportWidth, viewportHeight, 1100.0, 760.0),
        0.70,
        1.35
    );
    return static_cast<std::size_t>(std::max(
        1.0,
        static_cast<double>(baseBudget) * viewportScale
    ));
}

[[nodiscard]] double deepSkyLabelScore(
    const skygate::ephemeris::CelestialBody& body,
    const SkyRenderGlyph& glyph,
    const double viewportWidth,
    const double viewportHeight
)
{
    double score = 0.0;
    if (isMessierObject(body)) {
        score += 1000.0;
    } else if (hasCommonNameAlias(body)) {
        score += 700.0;
    }

    if (std::isfinite(body.visualMagnitude)) {
        score += std::clamp(14.0 - body.visualMagnitude, 0.0, 16.0) * 12.0;
    }

    if (body.deepSkyObject.has_value()) {
        score += std::clamp(
            body.deepSkyObject->majorAxisArcmin.value_or(0.0),
            0.0,
            120.0
        ) * 0.75;
    }

    const double centerX = viewportWidth * 0.5;
    const double centerY = viewportHeight * 0.5;
    const double normalizedDistanceSquared =
        skygate::core::squaredDistance2d(
            glyph.x,
            glyph.y,
            centerX,
            centerY
        ) / std::max(1.0, viewportWidth * viewportWidth + viewportHeight * viewportHeight);

    return score - (normalizedDistanceSquared * 120.0);
}

}  // namespace

namespace skygate::ui::internal {

QColor skyRenderLabelColorForBodyType(
    const skygate::ephemeris::CelestialBodyType type,
    const SkyThemeRenderPalette& renderTheme
)
{
    switch (type) {
    case skygate::ephemeris::CelestialBodyType::Sun:
        return renderTheme.labelSun;
    case skygate::ephemeris::CelestialBodyType::Moon:
        return renderTheme.labelMoon;
    case skygate::ephemeris::CelestialBodyType::Planet:
        return renderTheme.labelPlanet;
    case skygate::ephemeris::CelestialBodyType::Constellation:
        return renderTheme.labelConstellation;
    case skygate::ephemeris::CelestialBodyType::DeepSkyObject:
        return renderTheme.labelDeepSkyObject;
    case skygate::ephemeris::CelestialBodyType::Star:
        break;
    }

    return renderTheme.labelDefault;
}

skygate::core::Rect2d skyRenderLabelBounds(
    const double anchorX,
    const double anchorY,
    const std::string_view text
)
{
    constexpr double kLabelPaddingX = 12.0;
    constexpr double kLabelHeightPx = 20.0;
    constexpr double kLabelOffsetYPx = 8.0;
    constexpr double kApproximateGlyphWidthPx = 6.8;

    const double width = std::max(
        24.0,
        (static_cast<double>(text.size()) * kApproximateGlyphWidthPx) + kLabelPaddingX
    );
    const double left = anchorX - (width * 0.5);
    const double top = anchorY - kLabelHeightPx - kLabelOffsetYPx;
    return skygate::core::Rect2d {
        .left = left,
        .top = top,
        .right = left + width,
        .bottom = top + kLabelHeightPx
    };
}

bool skyRenderLabelFitsViewport(
    const skygate::core::Rect2d& bounds,
    const double viewportWidth,
    const double viewportHeight,
    const double edgeMarginPx
)
{
    return skygate::core::fitsWithin(bounds, viewportWidth, viewportHeight, edgeMarginPx);
}

void appendSkyRenderLabel(
    std::vector<SkyRenderLabel>& labels,
    QString kind,
    const double x,
    const double y,
    QString text,
    const QColor& color
)
{
    labels.push_back(SkyRenderLabel {
        .kind = std::move(kind),
        .x = x,
        .y = y,
        .text = std::move(text),
        .color = color
    });
}

void appendSkyRenderLabel(
    std::vector<SkyRenderLabel>& labels,
    const double x,
    const double y,
    const std::string_view text,
    const QColor& color
)
{
    appendSkyRenderLabel(
        labels,
        {},
        x,
        y,
        QString::fromStdString(std::string(text)),
        color
    );
}

double skyRenderDeepSkyHitRadius(const SkyRenderGlyph& glyph)
{
    return std::max({glyph.radiusXPx, glyph.radiusYPx, 8.0});
}

void appendBodyPointLabels(
    SkyRenderFrame& frame,
    const skygate::ephemeris::SkySnapshot& snapshot,
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
        const bool isConstellationLabel =
            body.type == skygate::ephemeris::CelestialBodyType::Constellation;
        const bool isSolarSystemLabel =
            body.type == skygate::ephemeris::CelestialBodyType::Planet
            || body.type == skygate::ephemeris::CelestialBodyType::Sun
            || body.type == skygate::ephemeris::CelestialBodyType::Moon;
        if (
            (!isConstellationLabel || !overlayLayers.constellationLabels)
            && (!isSolarSystemLabel || !overlayLayers.solarSystemLabels)
        ) {
            continue;
        }

        if (body.displayName.empty() || !seenLabels.insert(body.displayName).second) {
            continue;
        }

        if (
            point.x < edgeMarginPx
            || point.x > (viewportWidth - edgeMarginPx)
            || point.y < edgeMarginPx
            || point.y > (viewportHeight - edgeMarginPx)
        ) {
            continue;
        }

        const skygate::core::Rect2d bounds = skyRenderLabelBounds(
            point.x,
            point.y,
            body.displayName
        );
        appendSkyRenderLabel(
            frame.labels,
            point.x,
            point.y,
            body.displayName,
            skyRenderLabelColorForBodyType(body.type, renderTheme)
        );
        labelGrid.add(bounds);
    }
}

void appendDeepSkyLabels(
    SkyRenderFrame& frame,
    const skygate::ephemeris::SkySnapshot& snapshot,
    const skygate::core::ProjectionParams& projectionParams,
    const double viewportWidth,
    const double viewportHeight,
    const SkyThemeRenderPalette& renderTheme,
    const double edgeMarginPx,
    std::unordered_set<std::string_view>& seenLabels,
    skygate::core::RectOccupancyGrid& labelGrid
)
{
    const std::size_t labelBudget = deepSkyLabelBudget(
        projectionParams.fovDeg,
        viewportWidth,
        viewportHeight
    );
    if (labelBudget == 0U) {
        return;
    }

    std::vector<DeepSkyLabelCandidate> candidates;
    candidates.reserve(std::min(frame.glyphs.size(), labelBudget * 4U));
    for (std::size_t glyphIndex = 0; glyphIndex < frame.glyphs.size(); ++glyphIndex) {
        const auto& glyph = frame.glyphs[glyphIndex];
        const auto& body = snapshot.bodyAt(glyph.bodyIndex);
        if (
            body.displayName.empty()
            || seenLabels.contains(body.displayName)
            || !shouldConsiderDeepSkyLabel(body, projectionParams.fovDeg)
        ) {
            continue;
        }

        const double anchorY = glyph.y - skyRenderDeepSkyHitRadius(glyph);
        const skygate::core::Rect2d bounds = skyRenderLabelBounds(glyph.x, anchorY, body.displayName);
        if (!skyRenderLabelFitsViewport(bounds, viewportWidth, viewportHeight, edgeMarginPx)) {
            continue;
        }

        candidates.push_back(DeepSkyLabelCandidate {
            .glyphIndex = glyphIndex,
            .score = deepSkyLabelScore(body, glyph, viewportWidth, viewportHeight),
            .bounds = bounds
        });
    }

    std::sort(
        candidates.begin(),
        candidates.end(),
        [&frame, &snapshot](
            const DeepSkyLabelCandidate& lhs,
            const DeepSkyLabelCandidate& rhs
        ) {
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
            skyRenderLabelColorForBodyType(body.type, renderTheme)
        );
        ++acceptedCount;
    }
}

}  // namespace skygate::ui::internal
