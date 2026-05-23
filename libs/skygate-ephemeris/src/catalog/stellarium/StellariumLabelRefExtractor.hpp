#pragma once

#include "catalog/constellation/ConstellationData.hpp"

#include <QJsonObject>

#include <vector>

namespace skygate::ephemeris {

class StellariumLabelRefExtractor final {
public:
    [[nodiscard]] static std::vector<ConstellationLabelRef> extract(const QJsonObject& rootObject);
};

}  // namespace skygate::ephemeris
