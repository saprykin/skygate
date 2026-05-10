#pragma once

#include <string>
#include <utility>
#include <vector>

namespace skygate::ephemeris {

using ConstellationLineRef = std::pair<std::string, std::string>;
using ConstellationLabelRef = std::pair<std::string, std::vector<std::string>>;

class FallbackConstellationData final {
public:
    [[nodiscard]] std::vector<ConstellationLineRef> lineRefs() const;
    [[nodiscard]] std::vector<ConstellationLabelRef> labelRefs() const;
};

}  // namespace skygate::ephemeris
