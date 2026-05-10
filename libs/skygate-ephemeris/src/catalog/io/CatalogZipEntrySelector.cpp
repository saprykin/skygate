#include "catalog/io/CatalogZipEntrySelector.hpp"

#include "StringUtilities.hpp"

#include <string_view>

namespace skygate::ephemeris {
namespace {

[[nodiscard]] bool looksLikeCsvPath(const std::string_view path)
{
    if (path.empty()) {
        return false;
    }

    std::size_t leafStart = path.find_last_of('/');
    if (leafStart == std::string_view::npos) {
        leafStart = 0;
    } else {
        ++leafStart;
    }

    const std::string_view leafName = path.substr(leafStart);
    if (leafName.size() < 4U) {
        return false;
    }

    return strings::equalsIgnoreAsciiCase(
        leafName.substr(leafName.size() - 4U),
        ".csv"
    );
}

[[nodiscard]] bool isUsableEntry(const ZipEntryMetadata& entry)
{
    return !entry.isDirectory() && !entry.isEncrypted();
}

}  // namespace

const ZipEntryMetadata* CatalogZipEntrySelector::selectFirstCsvEntry(
    const std::span<const ZipEntryMetadata> entries
)
{
    const ZipEntryMetadata* firstCandidate = nullptr;
    for (const ZipEntryMetadata& entry : entries) {
        if (!isUsableEntry(entry)) {
            continue;
        }

        if (firstCandidate == nullptr) {
            firstCandidate = &entry;
        }

        if (looksLikeCsvPath(entry.path)) {
            return &entry;
        }
    }

    return firstCandidate;
}

}  // namespace skygate::ephemeris
