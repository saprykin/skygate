#include "StellariumAnchorGroupExtractor.hpp"
#include "StellariumHipParser.hpp"
#include "StringUtilities.hpp"
#include "catalog/CatalogIdentifier.hpp"

#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace skygate::ephemeris {
namespace {

QString prettifyConstellationId(QString id)
{
    id = StringUtilities::normalizedLabelText(id);
    if (id.isEmpty()) {
        return {};
    }

    const QString lower = id.toLower();
    QString title;
    title.reserve(lower.size());
    bool wordStart = true;
    for (const QChar c : lower) {
        if (c.isSpace()) {
            title.append(c);
            wordStart = true;
            continue;
        }

        title.append(wordStart ? c.toUpper() : c);
        wordStart = false;
    }

    return title;
}

QString constellationLabelFromEntry(const QString& fallbackId, const QJsonObject& entry)
{
    constexpr std::array<const char*, 3> kDirectNameFields = {{
        "name",
        "label",
        "english",
    }};
    for (const char* fieldName : kDirectNameFields) {
        const QJsonValue fieldValue = entry.value(fieldName);
        if (fieldValue.isString()) {
            const QString text = fieldValue.toString().trimmed();
            if (!text.isEmpty()) {
                return text;
            }
        }
    }

    const QJsonValue commonNameValue = entry.value("common_name");
    if (commonNameValue.isString()) {
        const QString text = commonNameValue.toString().trimmed();
        if (!text.isEmpty()) {
            return text;
        }
    } else if (commonNameValue.isObject()) {
        const QJsonObject commonNameObject = commonNameValue.toObject();
        constexpr std::array<const char*, 4> kCommonNameFields = {{
            "english",
            "name",
            "native",
            "iau",
        }};
        for (const char* fieldName : kCommonNameFields) {
            const QJsonValue fieldValue = commonNameObject.value(fieldName);
            if (!fieldValue.isString()) {
                continue;
            }
            const QString text = fieldValue.toString().trimmed();
            if (!text.isEmpty()) {
                return text;
            }
        }
    }

    const QJsonValue idValue = entry.value("id");
    if (idValue.isString()) {
        const QString prettified = prettifyConstellationId(idValue.toString());
        if (!prettified.isEmpty()) {
            return prettified;
        }
    }

    return prettifyConstellationId(fallbackId);
}

}  // namespace

std::vector<ConstellationAnchorGroup> StellariumAnchorGroupExtractor::extract(const QJsonObject& rootObject)
{
    std::vector<ConstellationAnchorGroup> anchorGroups;
    std::unordered_map<std::string, std::size_t> indexByLabelKey;

    const auto addAnchorGroup = [&anchorGroups,
                                 &indexByLabelKey](QString labelText, const std::vector<std::vector<int>>& polylines) {
        labelText = labelText.trimmed();
        if (labelText.isEmpty()) {
            return;
        }

        std::unordered_set<int> uniqueHipSet;
        std::vector<int> uniqueHips;
        for (const auto& polyline : polylines) {
            for (const int hip : polyline) {
                if (hip > 0 && uniqueHipSet.insert(hip).second) {
                    uniqueHips.push_back(hip);
                }
            }
        }
        if (uniqueHips.size() < 2U) {
            return;
        }
        std::sort(uniqueHips.begin(), uniqueHips.end());

        const std::string labelKey = StringUtilities::normalizedLookupKey(StringUtilities::toUtf8(labelText));
        auto indexIt = indexByLabelKey.find(labelKey);
        if (indexIt == indexByLabelKey.end()) {
            std::vector<std::string> hipIds;
            hipIds.reserve(uniqueHips.size());
            for (const int hip : uniqueHips) {
                hipIds.push_back(CatalogIdentifier::hipBodyId(static_cast<std::uint64_t>(hip)));
            }
            const std::size_t newIndex = anchorGroups.size();
            anchorGroups.emplace_back(StringUtilities::toUtf8(labelText), std::move(hipIds));
            indexByLabelKey.insert({labelKey, newIndex});
            return;
        }

        std::unordered_set<std::string> existingIds(
            anchorGroups[indexIt->second].second.begin(), anchorGroups[indexIt->second].second.end()
        );
        for (const int hip : uniqueHips) {
            const std::string hipId = CatalogIdentifier::hipBodyId(static_cast<std::uint64_t>(hip));
            if (existingIds.insert(hipId).second) {
                anchorGroups[indexIt->second].second.push_back(hipId);
            }
        }
    };

    const auto collectEntry = [&addAnchorGroup](const QJsonValue& entry, const QString& fallbackId) {
        std::vector<std::vector<int>> polylines;
        QString labelText;

        StellariumHipParser::collectHipPolylines(entry, polylines);
        if (entry.isObject()) {
            labelText = constellationLabelFromEntry(fallbackId, entry.toObject());
        } else if (entry.isArray()) {
            labelText = prettifyConstellationId(fallbackId);
        }

        addAnchorGroup(labelText, polylines);
    };

    StellariumHipParser::forEachConstellationEntry(
        rootObject,
        [&collectEntry](const QJsonValue& entry, const QString& fallbackId) { collectEntry(entry, fallbackId); }
    );

    return anchorGroups;
}

}  // namespace skygate::ephemeris
