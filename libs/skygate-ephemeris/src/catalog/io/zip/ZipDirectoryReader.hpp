#pragma once

#include "ZipEntryMetadata.hpp"

#include <optional>
#include <string_view>
#include <vector>

namespace skygate::ephemeris {

class ZipDirectoryReader final {
public:
    [[nodiscard]] static std::optional<std::vector<ZipEntryMetadata>> readEntries(std::string_view zipData);
};

}  // namespace skygate::ephemeris
