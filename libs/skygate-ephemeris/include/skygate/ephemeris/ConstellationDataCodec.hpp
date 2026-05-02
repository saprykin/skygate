#pragma once

#include "skygate/ephemeris/ConstellationData.hpp"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace skygate::ephemeris {

class ConstellationDataCodec final {
public:
    [[nodiscard]] static std::string serializeLineRows(
        std::span<const ConstellationLineRef> lineRefs
    );
    [[nodiscard]] static std::vector<ConstellationLineRef> parseLineRows(std::string_view rows);

    [[nodiscard]] static std::string serializeLabelRows(
        std::span<const ConstellationLabelRef> labelRefs
    );
    [[nodiscard]] static std::vector<ConstellationLabelRef> parseLabelRows(std::string_view rows);
};

}  // namespace skygate::ephemeris
