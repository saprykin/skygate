#pragma once

#include "catalog/io/zip/ZipDirectoryReader.hpp"

#include <span>

namespace skygate::ephemeris {

class CatalogZipEntrySelector final {
public:
    [[nodiscard]] static const ZipEntryMetadata* selectFirstCsvEntry(
        std::span<const ZipEntryMetadata> entries
    );
};

}  // namespace skygate::ephemeris
