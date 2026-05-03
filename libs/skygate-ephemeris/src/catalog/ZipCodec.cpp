#include "catalog/ZipCodec.hpp"

#include "catalog/CatalogZipEntrySelector.hpp"
#include "catalog/ZipDirectoryReader.hpp"
#include "catalog/ZipEntryExtractor.hpp"

#include <span>

namespace skygate::ephemeris {

std::optional<std::string> ZipCodec::extractFirstCsvEntry(const std::string_view zipData) const
{
    const auto entries = ZipDirectoryReader::readEntries(zipData);
    if (!entries.has_value()) {
        return std::nullopt;
    }

    const ZipEntryMetadata* const entry = CatalogZipEntrySelector::selectFirstCsvEntry(
        std::span<const ZipEntryMetadata>(*entries)
    );
    if (entry == nullptr) {
        return std::nullopt;
    }

    const auto entryData = ZipEntryExtractor::extract(zipData, *entry);
    if (!entryData.has_value() || entryData->empty()) {
        return std::nullopt;
    }

    return entryData;
}

}  // namespace skygate::ephemeris
