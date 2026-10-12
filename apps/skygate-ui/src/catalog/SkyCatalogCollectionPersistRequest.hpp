#pragma once

#include "SkyCatalogSourcePersistEntry.hpp"

#include <vector>

namespace skygate::ui::internal {

struct SkyCatalogCollectionPersistRequest final {
    std::vector<SkyCatalogSourcePersistEntry> sources;
};

}  // namespace skygate::ui::internal
