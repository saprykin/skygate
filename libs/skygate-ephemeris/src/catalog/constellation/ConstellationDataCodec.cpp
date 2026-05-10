#include "skygate/ephemeris/ConstellationDataCodec.hpp"

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

std::string ConstellationDataCodec::serializeLineRows(
    const std::span<const ConstellationLineRef> lineRefs
)
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

std::vector<ConstellationLineRef> ConstellationDataCodec::parseLineRows(
    const std::string_view rows
)
{
    std::vector<ConstellationLineRef> lineRefs;
    std::size_t cursor = 0;
    while (cursor < rows.size()) {
        const std::size_t newline = rows.find('\n', cursor);
        const std::size_t lineEnd = newline == std::string_view::npos ? rows.size() : newline;
        const std::string_view line = rows.substr(cursor, lineEnd - cursor);

        if (!line.empty()) {
            const std::size_t delimiter = line.find('|');
            if (delimiter != std::string_view::npos) {
                const std::string_view startId = line.substr(0, delimiter);
                const std::string_view endId = line.substr(delimiter + 1);
                if (!startId.empty() && !endId.empty()) {
                    lineRefs.emplace_back(std::string(startId), std::string(endId));
                }
            }
        }

        if (newline == std::string_view::npos) {
            break;
        }
        cursor = newline + 1U;
    }

    return lineRefs;
}

std::string ConstellationDataCodec::serializeLabelRows(
    const std::span<const ConstellationLabelRef> labelRefs
)
{
    std::string rows;
    rows.reserve(labelRefs.size() * 48U);
    for (const auto& labelRef : labelRefs) {
        if (labelRef.first.empty() || labelRef.second.empty()) {
            continue;
        }

        std::string sanitizedLabel = labelRef.first;
        sanitizeLabel(sanitizedLabel);
        if (sanitizedLabel.empty()) {
            continue;
        }

        std::string row = sanitizedLabel;
        row += '|';
        bool hasAnyHip = false;
        for (const std::string& hipId : labelRef.second) {
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

std::vector<ConstellationLabelRef> ConstellationDataCodec::parseLabelRows(
    const std::string_view rows
)
{
    std::vector<ConstellationLabelRef> labelRefs;
    std::size_t cursor = 0;
    while (cursor < rows.size()) {
        const std::size_t newline = rows.find('\n', cursor);
        const std::size_t lineEnd = newline == std::string_view::npos ? rows.size() : newline;
        const std::string_view line = rows.substr(cursor, lineEnd - cursor);

        if (!line.empty()) {
            const std::size_t delimiter = line.find('|');
            if (delimiter != std::string_view::npos) {
                const std::string_view label = line.substr(0, delimiter);
                const std::string_view hipList = line.substr(delimiter + 1);
                if (!label.empty() && !hipList.empty()) {
                    std::vector<std::string> hipIds;
                    std::size_t hipCursor = 0;
                    while (hipCursor < hipList.size()) {
                        const std::size_t comma = hipList.find(',', hipCursor);
                        const std::size_t hipEnd =
                            comma == std::string_view::npos ? hipList.size() : comma;
                        const std::string_view hipId = hipList.substr(
                            hipCursor,
                            hipEnd - hipCursor
                        );
                        if (!hipId.empty()) {
                            hipIds.emplace_back(hipId);
                        }
                        if (comma == std::string_view::npos) {
                            break;
                        }
                        hipCursor = comma + 1U;
                    }

                    if (!hipIds.empty()) {
                        labelRefs.emplace_back(std::string(label), std::move(hipIds));
                    }
                }
            }
        }

        if (newline == std::string_view::npos) {
            break;
        }
        cursor = newline + 1U;
    }

    return labelRefs;
}

}  // namespace skygate::ephemeris
