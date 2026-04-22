#include "SkyContextController.hpp"

#include "skygate/core/math/AngleMath.hpp"
#include "skygate/core/math/ProjectionMath.hpp"

#include <QHash>

#include <algorithm>
#include <cmath>
#include <optional>

namespace {

QString normalizedLookupKey(const QString& value)
{
    return value.trimmed().toCaseFolded();
}

QString normalizedLookupKey(const std::string& value)
{
    return normalizedLookupKey(QString::fromStdString(value));
}

bool hasFiniteHorizontal(const skygate::core::HorizontalCoordinate& horizontal)
{
    return std::isfinite(horizontal.altitudeDeg) && std::isfinite(horizontal.azimuthDeg);
}

const skygate::ephemeris::CelestialBodyState* findBodyStateById(
    const skygate::ephemeris::SkySnapshot& snapshot,
    const QString& targetId
)
{
    const QString normalizedTargetId = normalizedLookupKey(targetId);
    for (const auto& state : snapshot.states) {
        const auto& body = snapshot.bodyAt(state.bodyIndex);
        if (normalizedLookupKey(body.id) == normalizedTargetId) {
            return &state;
        }
    }

    return nullptr;
}

std::optional<skygate::core::HorizontalCoordinate> findConstellationLabelCenter(
    const skygate::ephemeris::SkySnapshot& snapshot,
    const std::span<const SkyContextController::ConstellationLabelRef> labelRefs,
    const QString& targetId
)
{
    const QString normalizedTargetId = normalizedLookupKey(targetId);
    const auto labelRefIt = std::find_if(
        labelRefs.begin(),
        labelRefs.end(),
        [&normalizedTargetId](const SkyContextController::ConstellationLabelRef& labelRef) {
            return normalizedLookupKey(labelRef.first) == normalizedTargetId;
        }
    );
    if (labelRefIt == labelRefs.end()) {
        return std::nullopt;
    }

    QHash<QString, skygate::core::HorizontalCoordinate> horizontalByBodyId;
    horizontalByBodyId.reserve(static_cast<qsizetype>(snapshot.states.size()));
    for (const auto& state : snapshot.states) {
        const auto& body = snapshot.bodyAt(state.bodyIndex);
        horizontalByBodyId.insert(normalizedLookupKey(body.id), state.horizontal);
    }

    skygate::core::ProjectionMath::Vec3 sum {0.0, 0.0, 0.0};
    int validAnchorCount = 0;
    for (const std::string& hipId : labelRefIt->second) {
        const auto horizontalIt = horizontalByBodyId.constFind(normalizedLookupKey(hipId));
        if (horizontalIt == horizontalByBodyId.cend() || !hasFiniteHorizontal(*horizontalIt)) {
            continue;
        }

        const auto vector = skygate::core::ProjectionMath::horizontalToUnitVector(*horizontalIt);
        sum[0] += vector[0];
        sum[1] += vector[1];
        sum[2] += vector[2];
        ++validAnchorCount;
    }

    if (validAnchorCount == 0) {
        return std::nullopt;
    }

    const auto normalizedVector = skygate::core::ProjectionMath::normalize(sum);
    if (skygate::core::ProjectionMath::length(normalizedVector) <= 0.0) {
        return std::nullopt;
    }

    return skygate::core::HorizontalCoordinate {
        .altitudeDeg = skygate::core::AngleMath::toDegrees(std::asin(std::clamp(
            normalizedVector[2],
            -1.0,
            1.0
        ))),
        .azimuthDeg = skygate::core::AngleMath::normalizeDegrees(
            skygate::core::AngleMath::toDegrees(
                std::atan2(normalizedVector[0], normalizedVector[1])
            )
        )
    };
}

}  // namespace

bool SkyContextController::searchToolbarCollapsed() const noexcept
{
    return m_searchToolbarCollapsed;
}

void SkyContextController::setSearchToolbarCollapsed(const bool searchToolbarCollapsed)
{
    if (m_searchToolbarCollapsed == searchToolbarCollapsed) {
        return;
    }

    m_searchToolbarCollapsed = searchToolbarCollapsed;
    if (m_searchToolbarCollapsed) {
        clearSelectedSearchTarget();
    }
    emit searchToolbarCollapsedChanged();
}

bool SkyContextController::focusSearchTarget(const QString& targetKind, const QString& targetId)
{
    const auto* engine = ephemerisEngine();
    if (
        engine == nullptr
        || targetKind.trimmed().isEmpty()
        || targetId.trimmed().isEmpty()
    ) {
        return false;
    }

    const auto snapshot = engine->compute(m_skyContext);
    const QString normalizedTargetKind = normalizedLookupKey(targetKind);
    if (normalizedTargetKind == "body") {
        const auto* bodyState = findBodyStateById(snapshot, targetId);
        if (bodyState == nullptr || !hasFiniteHorizontal(bodyState->horizontal)) {
            return false;
        }

        setSelectedSearchTarget("body", targetId);
        setViewCenter(bodyState->horizontal.altitudeDeg, bodyState->horizontal.azimuthDeg);
        return true;
    }

    if (normalizedTargetKind == "constellationlabel") {
        const auto center = findConstellationLabelCenter(snapshot, constellationLabelRefs(), targetId);
        if (!center.has_value()) {
            return false;
        }

        setSelectedSearchTarget("constellationLabel", targetId);
        setViewCenter(center->altitudeDeg, center->azimuthDeg);
        return true;
    }

    return false;
}
