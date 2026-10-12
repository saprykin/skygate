#include "SkySearchController.hpp"

namespace skygate::ui::internal {

bool SkySearchController::toolbarCollapsed() const noexcept
{
    return m_state.toolbarCollapsed;
}

QString SkySearchController::selectedTargetKind() const
{
    return m_state.selectedTargetKind;
}

QString SkySearchController::selectedTargetId() const
{
    return m_state.selectedTargetId;
}

bool SkySearchController::hasTrackedTarget() const
{
    return !m_state.trackedTargetKind.trimmed().isEmpty() && !m_state.trackedTargetId.trimmed().isEmpty();
}

QString SkySearchController::trackedTargetKind() const
{
    return m_state.trackedTargetKind;
}

QString SkySearchController::trackedTargetId() const
{
    return m_state.trackedTargetId;
}

QString SkySearchController::trackedTargetDisplayText() const
{
    return m_state.trackedTargetDisplayText;
}

bool SkySearchController::setToolbarCollapsed(const bool toolbarCollapsed) noexcept
{
    if (m_state.toolbarCollapsed == toolbarCollapsed) {
        return false;
    }

    m_state.toolbarCollapsed = toolbarCollapsed;
    return true;
}

bool SkySearchController::setSelectedTarget(const QString& targetKind, const QString& targetId)
{
    const QString normalizedTargetKind = targetKind.trimmed();
    const QString normalizedTargetId = targetId.trimmed();
    if (m_state.selectedTargetKind == normalizedTargetKind && m_state.selectedTargetId == normalizedTargetId) {
        return false;
    }

    m_state.selectedTargetKind = normalizedTargetKind;
    m_state.selectedTargetId = normalizedTargetId;
    return true;
}

bool SkySearchController::setTrackedTarget(
    const QString& targetKind, const QString& targetId, const QString& displayText
)
{
    const QString normalizedTargetKind = targetKind.trimmed();
    const QString normalizedTargetId = targetId.trimmed();
    const QString normalizedDisplayText = displayText.trimmed();
    if (m_state.trackedTargetKind == normalizedTargetKind && m_state.trackedTargetId == normalizedTargetId
        && m_state.trackedTargetDisplayText == normalizedDisplayText) {
        return false;
    }

    m_state.trackedTargetKind = normalizedTargetKind;
    m_state.trackedTargetId = normalizedTargetId;
    m_state.trackedTargetDisplayText = normalizedDisplayText;
    return true;
}

}  // namespace skygate::ui::internal
