#include "TrailRenderQuery.hpp"
#include "SkyRenderLabel.hpp"
#include "SkyRenderLine.hpp"

#include <algorithm>
#include <cmath>

namespace skygate::ui::tests {

int TrailRenderQuery::countLabelsOfKind(const std::vector<SkyRenderLabel>& labels, const QString& kind)
{
    int count = 0;
    for (const SkyRenderLabel& label : labels) {
        if (label.kind == kind) {
            ++count;
        }
    }
    return count;
}

bool TrailRenderQuery::lineTouchesPoint(
    const SkyRenderLine& line, const double x, const double y, const double tolerancePx
)
{
    const auto closeToPoint = [x, y, tolerancePx](const double lineX, const double lineY) {
        return std::hypot(lineX - x, lineY - y) <= tolerancePx;
    };
    return closeToPoint(line.x1, line.y1) || closeToPoint(line.x2, line.y2);
}

double TrailRenderQuery::lineLength(const SkyRenderLine& line)
{
    return std::hypot(line.x2 - line.x1, line.y2 - line.y1);
}

}  // namespace skygate::ui::tests
