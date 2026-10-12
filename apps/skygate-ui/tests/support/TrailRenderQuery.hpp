#pragma once

#include "SkyRenderFrame.hpp"
#include "SkyRenderLabel.hpp"
#include "SkyRenderLine.hpp"

#include <QString>

#include <vector>

namespace skygate::ui::tests {

// Read-only queries over rendered frame content used by the trail assertions.
class TrailRenderQuery final {
public:
    [[nodiscard]] static int countLabelsOfKind(const std::vector<SkyRenderLabel>& labels, const QString& kind);
    [[nodiscard]] static bool lineTouchesPoint(const SkyRenderLine& line, double x, double y, double tolerancePx);
    [[nodiscard]] static double lineLength(const SkyRenderLine& line);
};

}  // namespace skygate::ui::tests
