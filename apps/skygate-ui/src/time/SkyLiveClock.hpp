#pragma once

#include "skygate/core/Types.hpp"

#include <QElapsedTimer>

class SkyLiveClock final {
public:
    void start(const skygate::core::UtcTimePoint& anchorUtc);
    void stop();
    [[nodiscard]] bool isRunning() const noexcept;
    [[nodiscard]] skygate::core::UtcTimePoint currentUtc() const;
    void resetAnchor(const skygate::core::UtcTimePoint& anchorUtc);

private:
    skygate::core::UtcTimePoint m_anchorUtc{};
    QElapsedTimer m_elapsedTimer;
};
