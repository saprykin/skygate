#pragma once

#include "catalog/io/zip/ZipDirectoryReader.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace skygate::ephemeris {

class ZipEntryExtractor final {
public:
    [[nodiscard]] static std::optional<std::string> extract(
        std::string_view zipData,
        const ZipEntryMetadata& entry
    );
};

}  // namespace skygate::ephemeris
