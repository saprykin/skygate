#include "SkyContextCatalogCodec.hpp"
#include "catalog/constellation/ConstellationDataCodec.hpp"

namespace skygate::ui::internal {

QByteArray
SkyContextCatalogCodec::serializeConstellationLineRows(const std::vector<std::pair<std::string, std::string>>& lineRefs)
{
    return QByteArray::fromStdString(skygate::ephemeris::ConstellationDataCodec::serializeLineRows(lineRefs));
}

std::vector<std::pair<std::string, std::string>>
SkyContextCatalogCodec::parseConstellationLineRows(const std::string_view rows)
{
    return skygate::ephemeris::ConstellationDataCodec::parseLineRows(rows);
}

QByteArray SkyContextCatalogCodec::serializeConstellationAnchorGroupRows(
    const std::vector<std::pair<std::string, std::vector<std::string>>>& anchorGroups
)
{
    return QByteArray::fromStdString(
        skygate::ephemeris::ConstellationDataCodec::serializeAnchorGroupRows(anchorGroups)
    );
}

std::vector<std::pair<std::string, std::vector<std::string>>>
SkyContextCatalogCodec::parseConstellationAnchorGroupRows(const std::string_view rows)
{
    return skygate::ephemeris::ConstellationDataCodec::parseAnchorGroupRows(rows);
}

}  // namespace skygate::ui::internal
