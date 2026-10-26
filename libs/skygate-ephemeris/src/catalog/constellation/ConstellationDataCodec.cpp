#include "ConstellationDataCodec.hpp"
#include "StringUtilities.hpp"

#include <algorithm>
#include <optional>
#include <utility>

namespace skygate::ephemeris {
namespace {

constexpr char kFieldDelimiter = '|';

void sanitizeLabel(std::string& label)
{
    std::replace(label.begin(), label.end(), '|', '/');
    std::replace(label.begin(), label.end(), '\n', ' ');
    std::replace(label.begin(), label.end(), '\r', ' ');

    label = std::string(StringUtilities::trimAsciiWhitespace(label));
}

// Every row of the codec is one non-empty left field, the delimiter, one
// non-empty right field, and a line terminator. Serializers skip a row that
// carries an empty side instead of writing a frame the parsers would reject.
void appendRow(std::string& rows, const std::string_view left, const std::string_view right)
{
    if (left.empty() || right.empty()) {
        return;
    }

    rows.append(left);
    rows += kFieldDelimiter;
    rows.append(right);
    rows += '\n';
}

// The two fields of one framed row, or nothing when the row has no delimiter or
// an empty side. Both row families are parsed through the same framing rule.
[[nodiscard]] std::optional<std::pair<std::string_view, std::string_view>> splitRow(const std::string_view line)
{
    const std::size_t delimiter = line.find(kFieldDelimiter);
    if (delimiter == std::string_view::npos) {
        return std::nullopt;
    }

    const std::string_view left = line.substr(0U, delimiter);
    const std::string_view right = line.substr(delimiter + 1U);
    if (left.empty() || right.empty()) {
        return std::nullopt;
    }

    return std::pair{left, right};
}

}  // namespace

std::string ConstellationDataCodec::serializeLineRows(const std::span<const ConstellationLineRef> lineRefs)
{
    std::string rows;
    rows.reserve(lineRefs.size() * 24U);
    for (const auto& lineRef : lineRefs) {
        appendRow(rows, lineRef.first, lineRef.second);
    }

    return rows;
}

std::vector<ConstellationLineRef> ConstellationDataCodec::parseLineRows(const std::string_view rows)
{
    std::vector<ConstellationLineRef> lineRefs;
    for (const std::string_view line : StringUtilities::splitView(rows, '\n')) {
        const std::optional<std::pair<std::string_view, std::string_view>> fields = splitRow(line);
        if (!fields.has_value()) {
            continue;
        }

        lineRefs.emplace_back(std::string(fields->first), std::string(fields->second));
    }

    return lineRefs;
}

std::string
ConstellationDataCodec::serializeAnchorGroupRows(const std::span<const ConstellationAnchorGroup> anchorGroups)
{
    std::string rows;
    rows.reserve(anchorGroups.size() * 48U);
    for (const auto& anchorGroup : anchorGroups) {
        std::string sanitizedLabel = anchorGroup.first;
        sanitizeLabel(sanitizedLabel);

        std::string hipList;
        for (const std::string& hipId : anchorGroup.second) {
            if (hipId.empty()) {
                continue;
            }
            if (!hipList.empty()) {
                hipList += ',';
            }
            hipList += hipId;
        }

        appendRow(rows, sanitizedLabel, hipList);
    }

    return rows;
}

std::vector<ConstellationAnchorGroup> ConstellationDataCodec::parseAnchorGroupRows(const std::string_view rows)
{
    std::vector<ConstellationAnchorGroup> anchorGroups;
    for (const std::string_view line : StringUtilities::splitView(rows, '\n')) {
        const std::optional<std::pair<std::string_view, std::string_view>> fields = splitRow(line);
        if (!fields.has_value()) {
            continue;
        }

        std::vector<std::string> hipIds;
        for (const std::string_view hipId : StringUtilities::splitView(fields->second, ',')) {
            hipIds.emplace_back(hipId);
        }
        if (hipIds.empty()) {
            continue;
        }

        anchorGroups.emplace_back(std::string(fields->first), std::move(hipIds));
    }

    return anchorGroups;
}

}  // namespace skygate::ephemeris
