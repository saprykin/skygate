#include "SkyCatalogRelatedDataCodec.hpp"
#include "catalog/constellation/ConstellationDataCodec.hpp"

namespace skygate::ui::internal {

QByteArray SkyCatalogRelatedDataCodec::serializeConstellationLineRows(
    const std::vector<std::pair<std::string, std::string>>& lineRefs
)
{
    return QByteArray::fromStdString(skygate::ephemeris::ConstellationDataCodec::serializeLineRows(lineRefs));
}

std::vector<std::pair<std::string, std::string>>
SkyCatalogRelatedDataCodec::parseConstellationLineRows(const std::string_view rows)
{
    return skygate::ephemeris::ConstellationDataCodec::parseLineRows(rows);
}

QByteArray SkyCatalogRelatedDataCodec::serializeConstellationAnchorGroupRows(
    const std::vector<std::pair<std::string, std::vector<std::string>>>& anchorGroups
)
{
    return QByteArray::fromStdString(
        skygate::ephemeris::ConstellationDataCodec::serializeAnchorGroupRows(anchorGroups)
    );
}

std::vector<std::pair<std::string, std::vector<std::string>>>
SkyCatalogRelatedDataCodec::parseConstellationAnchorGroupRows(const std::string_view rows)
{
    return skygate::ephemeris::ConstellationDataCodec::parseAnchorGroupRows(rows);
}

}  // namespace skygate::ui::internal
