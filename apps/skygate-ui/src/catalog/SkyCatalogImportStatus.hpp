#pragma once

#include <QString>

namespace skygate::ui::internal {

// A catalog import notification pairs its display text with the phase that
// produced it, so control flow reads the phase instead of matching the
// wording.
struct SkyCatalogImportStatus final {
    enum class Phase {
        Downloading,
        Processing,
        Failed,
    };

    QString text;
    Phase phase = Phase::Downloading;

    [[nodiscard]] bool isProcessing() const noexcept
    {
        return phase == Phase::Processing;
    }
};

}  // namespace skygate::ui::internal
