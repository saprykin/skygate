#pragma once

#include <QByteArray>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace skygate::ui::internal {

class SkyCatalogRelatedDataCodec final {
public:
    [[nodiscard]] static QByteArray
    serializeConstellationLineRows(const std::vector<std::pair<std::string, std::string>>& lineRefs);
    [[nodiscard]] static std::vector<std::pair<std::string, std::string>>
    parseConstellationLineRows(std::string_view rows);

    [[nodiscard]] static QByteArray serializeConstellationAnchorGroupRows(
        const std::vector<std::pair<std::string, std::vector<std::string>>>& anchorGroups
    );
    [[nodiscard]] static std::vector<std::pair<std::string, std::vector<std::string>>>
    parseConstellationAnchorGroupRows(std::string_view rows);
};

}  // namespace skygate::ui::internal
