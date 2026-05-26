#pragma once

#include "catalog/constellation/ConstellationData.hpp"

#include <QJsonObject>

#include <vector>

namespace skygate::ephemeris {

class StellariumAnchorGroupExtractor final {
public:
    [[nodiscard]] static std::vector<ConstellationAnchorGroup> extract(const QJsonObject& rootObject);
};

}  // namespace skygate::ephemeris
