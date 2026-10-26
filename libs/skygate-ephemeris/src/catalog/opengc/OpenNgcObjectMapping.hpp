#pragma once

#include "DeepSkyObjectInfo.hpp"
#include "catalog/CatalogIdentifier.hpp"

#include <string>
#include <vector>

namespace skygate::ephemeris {

struct OpenNgcObjectMapping final {
    std::string id;
    std::string displayName;
    std::string sourceRecordId;
    DeepSkyObjectInfo::Kind kind = DeepSkyObjectInfo::Kind::Unknown;
    std::vector<CatalogIdentifier> externalIdentifiers;
    std::vector<std::string> aliases;
};

}  // namespace skygate::ephemeris
