#include "ZipCodec.hpp"
#include "catalog/io/CatalogZipEntrySelector.hpp"

#include <utility>

namespace skygate::ephemeris {

std::optional<std::string> ZipCodec::extractFirstCsvEntry(const std::string_view zipData) const
{
    CatalogZipEntrySelection selection = CatalogZipEntrySelector::select(zipData, std::nullopt);
    if (selection.status != CatalogZipEntrySelection::Status::Selected) {
        return std::nullopt;
    }
    return std::move(selection.payload);
}

}  // namespace skygate::ephemeris
