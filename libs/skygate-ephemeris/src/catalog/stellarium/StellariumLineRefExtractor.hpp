#pragma once

#include "skygate/ephemeris/ConstellationData.hpp"

#include <QJsonObject>

#include <vector>

namespace skygate::ephemeris {

class StellariumLineRefExtractor final {
public:
    [[nodiscard]] static std::vector<ConstellationLineRef> extract(const QJsonObject& rootObject);
};

}  // namespace skygate::ephemeris
