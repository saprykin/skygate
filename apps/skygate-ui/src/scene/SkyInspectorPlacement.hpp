#pragma once

#include "SkySelectedObjectInspector.hpp"

#include <QString>

#include <cmath>

// Tracks which object the inspector belongs to and where a pinned inspector
// sits. The pin, move, and clear transitions report whether the rendered
// inspector can follow in place or the scene frame has to be rebuilt.
class SkyInspectorPlacement final {
public:
    enum class Change {
        Ignored,
        AppliedInPlace,
        RequiresRebuild,
    };

    [[nodiscard]] const QString& targetId() const noexcept
    {
        return m_targetId;
    }

    [[nodiscard]] bool pinned() const noexcept
    {
        return m_pinned;
    }

    [[nodiscard]] double pinnedX() const noexcept
    {
        return m_pinnedX;
    }

    [[nodiscard]] double pinnedY() const noexcept
    {
        return m_pinnedY;
    }

    [[nodiscard]] bool hasSelection() const noexcept
    {
        return !m_targetId.isEmpty() || m_pinned;
    }

    void selectTarget(const QString& targetId)
    {
        m_targetId = targetId;
        m_pinned = false;
    }

    void clear()
    {
        m_targetId.clear();
        m_pinned = false;
    }

    [[nodiscard]] Change pin(const bool pinned, const SkySelectedObjectInspector& inspector)
    {
        if (m_pinned == pinned) {
            return Change::Ignored;
        }

        if (!pinned) {
            m_pinned = false;
            return Change::RequiresRebuild;
        }

        if (!inspector.visible) {
            return Change::Ignored;
        }

        m_pinnedX = inspector.x;
        m_pinnedY = inspector.y;
        m_pinned = true;
        return Change::AppliedInPlace;
    }

    [[nodiscard]] Change move(const double x, const double y, const SkySelectedObjectInspector& inspector)
    {
        if (!std::isfinite(x) || !std::isfinite(y)) {
            return Change::Ignored;
        }

        m_pinnedX = x;
        m_pinnedY = y;
        m_pinned = true;
        return inspector.visible ? Change::AppliedInPlace : Change::RequiresRebuild;
    }

private:
    QString m_targetId;
    double m_pinnedX = 0.0;
    double m_pinnedY = 0.0;
    bool m_pinned = false;
};
