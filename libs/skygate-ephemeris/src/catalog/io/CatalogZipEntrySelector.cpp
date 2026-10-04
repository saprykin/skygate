#include "CatalogZipEntrySelector.hpp"
#include "CatalogPayloadFormatDetector.hpp"
#include "StringUtilities.hpp"
#include "catalog/CatalogSourceType.hpp"
#include "zip/ZipDirectoryReader.hpp"
#include "zip/ZipEntryExtractor.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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

    return StringUtilities::equalsIgnoreAsciiCase(leafName.substr(leafName.size() - 4U), ".csv");
}

[[nodiscard]] bool isUsableEntry(const ZipEntryMetadata& entry)
{
    return !entry.isDirectory() && !entry.isEncrypted();
}

[[nodiscard]] const ZipEntryMetadata*
findEntryByPath(const std::span<const ZipEntryMetadata> entries, const std::string_view path)
{
    for (const ZipEntryMetadata& entry : entries) {
        if (entry.path == path) {
            return &entry;
        }
    }
    return nullptr;
}

}  // namespace

CatalogZipEntrySelection
CatalogZipEntrySelector::select(const std::string_view zipData, const std::optional<std::string>& memberSelector)
{
    CatalogZipEntrySelection selection;

    const auto entries = ZipDirectoryReader::readEntries(zipData);
    if (!entries.has_value()) {
        selection.status = CatalogZipEntrySelection::Status::InvalidArchive;
        return selection;
    }

    const std::span<const ZipEntryMetadata> entrySpan(*entries);

    if (memberSelector.has_value()) {
        const std::string_view requestedPath(*memberSelector);
        selection.selectedPath = *memberSelector;

        const ZipEntryMetadata* const entry = findEntryByPath(entrySpan, requestedPath);
        if (entry == nullptr) {
            selection.status = CatalogZipEntrySelection::Status::MissingMember;
            return selection;
        }
        if (entry->isDirectory() || entry->isEncrypted()) {
            selection.status = CatalogZipEntrySelection::Status::UnusableMember;
            return selection;
        }

        const auto payload = ZipEntryExtractor::extract(zipData, *entry);
        if (!payload.has_value()) {
            selection.status = CatalogZipEntrySelection::Status::UnusableMember;
            return selection;
        }

        selection.status = CatalogZipEntrySelection::Status::Selected;
        selection.payload = std::move(*payload);
        return selection;
    }

    std::vector<const ZipEntryMetadata*> csvCandidates;
    std::vector<const ZipEntryMetadata*> otherCandidates;
    csvCandidates.reserve(entrySpan.size());
    otherCandidates.reserve(entrySpan.size());
    for (const ZipEntryMetadata& entry : entrySpan) {
        if (!isUsableEntry(entry)) {
            continue;
        }
        if (looksLikeCsvPath(entry.path)) {
            csvCandidates.push_back(&entry);
        } else {
            otherCandidates.push_back(&entry);
        }
    }

    const std::vector<const ZipEntryMetadata*>& candidates = !csvCandidates.empty() ? csvCandidates : otherCandidates;

    if (candidates.size() == 1U) {
        const ZipEntryMetadata* const entry = candidates.front();
        const auto payload = ZipEntryExtractor::extract(zipData, *entry);
        if (!payload.has_value()) {
            selection.status = CatalogZipEntrySelection::Status::NoReadableEntry;
            return selection;
        }
        selection.status = CatalogZipEntrySelection::Status::Selected;
        selection.selectedPath = entry->path;
        selection.payload = std::move(*payload);
        return selection;
    }

    if (candidates.empty()) {
        selection.status = CatalogZipEntrySelection::Status::NoReadableEntry;
        return selection;
    }

    const ZipEntryMetadata* selectedEntry = nullptr;
    std::string selectedPayload;
    for (const ZipEntryMetadata* const entry : candidates) {
        auto payload = ZipEntryExtractor::extract(zipData, *entry);
        if (!payload.has_value()) {
            continue;
        }
        if (CatalogPayloadFormatDetector::detect(*payload) == CatalogSourceType::Unknown) {
            continue;
        }
        if (selectedEntry != nullptr) {
            selection.status = CatalogZipEntrySelection::Status::AmbiguousMember;
            return selection;
        }
        selectedEntry = entry;
        selectedPayload = std::move(*payload);
    }

    if (selectedEntry == nullptr) {
        selection.status = CatalogZipEntrySelection::Status::NoSupportedMember;
        return selection;
    }

    selection.status = CatalogZipEntrySelection::Status::Selected;
    selection.selectedPath = selectedEntry->path;
    selection.payload = std::move(selectedPayload);
    return selection;
}

}  // namespace skygate::ephemeris
