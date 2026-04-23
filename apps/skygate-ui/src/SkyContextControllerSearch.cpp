#include "SkyContextController.hpp"

#include "skygate/ephemeris/ConstellationReferenceCalculator.hpp"

#include <cmath>

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
        const auto center = skygate::ephemeris::ConstellationReferenceCalculator::labelCenter(
            snapshot,
            constellationLabelRefs(),
            targetId.toStdString()
        );
        if (!center.has_value()) {
            return false;
        }

        setSelectedSearchTarget("constellationLabel", targetId);
        setViewCenter(center->altitudeDeg, center->azimuthDeg);
        return true;
    }

    return false;
}
