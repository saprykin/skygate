#include "ConstellationDataCodec.hpp"
#include "StringUtilities.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

namespace skygate::ephemeris {
namespace {

void sanitizeLabel(std::string& label)
{
    std::replace(label.begin(), label.end(), '|', '/');
    std::replace(label.begin(), label.end(), '\n', ' ');
    std::replace(label.begin(), label.end(), '\r', ' ');

    const auto first = std::find_if(label.begin(), label.end(), [](const unsigned char character) {
        return !std::isspace(character);
    });
    const auto last = std::find_if(label.rbegin(), label.rend(), [](const unsigned char character) {
                          return !std::isspace(character);
                      }).base();

    if (first >= last) {
        label.clear();
        return;
    }

    label = std::string(first, last);
}

}  // namespace

std::string ConstellationDataCodec::serializeLineRows(const std::span<const ConstellationLineRef> lineRefs)
{
    std::string rows;
    rows.reserve(lineRefs.size() * 24U);
    for (const auto& lineRef : lineRefs) {
        if (lineRef.first.empty() || lineRef.second.empty()) {
            continue;
        }

        rows += lineRef.first;
        rows += '|';
        rows += lineRef.second;
        rows += '\n';
    }

    return rows;
}

std::vector<ConstellationLineRef> ConstellationDataCodec::parseLineRows(const std::string_view rows)
{
    std::vector<ConstellationLineRef> lineRefs;
    for (const std::string_view line : StringUtilities::splitView(rows, '\n')) {
        const std::size_t delimiter = line.find('|');
        if (delimiter == std::string_view::npos) {
            continue;
        }

        const std::string_view startId = line.substr(0, delimiter);
        const std::string_view endId = line.substr(delimiter + 1);
        if (startId.empty() || endId.empty()) {
            continue;
        }

        lineRefs.emplace_back(std::string(startId), std::string(endId));
    }

    return lineRefs;
}

std::string
ConstellationDataCodec::serializeAnchorGroupRows(const std::span<const ConstellationAnchorGroup> anchorGroups)
{
    std::string rows;
    rows.reserve(anchorGroups.size() * 48U);
    for (const auto& anchorGroup : anchorGroups) {
        if (anchorGroup.first.empty() || anchorGroup.second.empty()) {
            continue;
        }

        std::string sanitizedLabel = anchorGroup.first;
        sanitizeLabel(sanitizedLabel);
        if (sanitizedLabel.empty()) {
            continue;
        }

        std::string row = sanitizedLabel;
        row += '|';
        bool hasAnyHip = false;
        for (const std::string& hipId : anchorGroup.second) {
            if (hipId.empty()) {
                continue;
            }
            if (hasAnyHip) {
                row += ',';
            }
            row += hipId;
            hasAnyHip = true;
        }

        if (!hasAnyHip) {
            continue;
        }

        rows += row;
        rows += '\n';
    }

    return rows;
}

std::vector<ConstellationAnchorGroup> ConstellationDataCodec::parseAnchorGroupRows(const std::string_view rows)
{
    std::vector<ConstellationAnchorGroup> anchorGroups;
    for (const std::string_view line : StringUtilities::splitView(rows, '\n')) {
        const std::size_t delimiter = line.find('|');
        if (delimiter == std::string_view::npos) {
            continue;
        }

        const std::string_view label = line.substr(0, delimiter);
        const std::string_view hipList = line.substr(delimiter + 1);
        if (label.empty() || hipList.empty()) {
            continue;
        }

        std::vector<std::string> hipIds;
        for (const std::string_view hipId : StringUtilities::splitView(hipList, ',')) {
            hipIds.emplace_back(hipId);
        }
        if (!hipIds.empty()) {
            anchorGroups.emplace_back(std::string(label), std::move(hipIds));
        }
    }

    return anchorGroups;
}

}  // namespace skygate::ephemeris
