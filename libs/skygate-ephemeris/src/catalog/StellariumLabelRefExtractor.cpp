#include "catalog/StellariumLabelRefExtractor.hpp"

#include "catalog/StellariumHipParser.hpp"
#include "common/StringUtilities.hpp"

#include <QJsonArray>
#include <QJsonValue>
#include <QString>

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace skygate::ephemeris {
namespace {

QString prettifyConstellationId(QString id)
{
    id = id.trimmed();
    id.replace('_', ' ');
    id.replace('-', ' ');
    id = id.simplified();
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

std::vector<ConstellationLabelRef> StellariumLabelRefExtractor::extract(
    const QJsonObject& rootObject
)
{
    std::vector<ConstellationLabelRef> labelRefs;
    std::unordered_map<std::string, std::size_t> indexByLabelKey;

    const auto addLabelEntry = [&labelRefs, &indexByLabelKey](
        QString labelText,
        const std::vector<std::vector<int>>& polylines
    ) {
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

        const std::string labelKey = strings::normalizedLookupKey(labelText.toStdString());
        auto indexIt = indexByLabelKey.find(labelKey);
        if (indexIt == indexByLabelKey.end()) {
            std::vector<std::string> hipIds;
            hipIds.reserve(uniqueHips.size());
            for (const int hip : uniqueHips) {
                hipIds.push_back("hip_" + std::to_string(hip));
            }
            const std::size_t newIndex = labelRefs.size();
            labelRefs.emplace_back(labelText.toStdString(), std::move(hipIds));
            indexByLabelKey.insert({labelKey, newIndex});
            return;
        }

        std::unordered_set<std::string> existingIds(
            labelRefs[indexIt->second].second.begin(),
            labelRefs[indexIt->second].second.end()
        );
        for (const int hip : uniqueHips) {
            const std::string hipId = "hip_" + std::to_string(hip);
            if (existingIds.insert(hipId).second) {
                labelRefs[indexIt->second].second.push_back(hipId);
            }
        }
    };

    const auto collectEntry = [&addLabelEntry](const QJsonValue& entry, const QString& fallbackId) {
        std::vector<std::vector<int>> polylines;
        QString labelText;

        if (entry.isObject()) {
            const QJsonObject object = entry.toObject();
            const QJsonValue linesValue = object.value("lines");
            const QJsonValue lineValue = object.value("line");
            if (!linesValue.isUndefined()) {
                StellariumHipParser::collectHipPolylines(linesValue, polylines);
            }
            if (!lineValue.isUndefined()) {
                StellariumHipParser::collectHipPolylines(lineValue, polylines);
            }
            labelText = constellationLabelFromEntry(fallbackId, object);
        } else if (entry.isArray()) {
            StellariumHipParser::collectHipPolylines(entry, polylines);
            labelText = prettifyConstellationId(fallbackId);
        }

        addLabelEntry(labelText, polylines);
    };

    const QJsonValue constellationsValue = rootObject.value("constellations");
    if (constellationsValue.isArray()) {
        for (const QJsonValue& entry : constellationsValue.toArray()) {
            collectEntry(entry, {});
        }
    } else if (constellationsValue.isObject()) {
        const QJsonObject constellationsObject = constellationsValue.toObject();
        const QJsonValue linesValue = constellationsObject.value("lines");
        const QJsonValue lineValue = constellationsObject.value("line");
        if (!linesValue.isUndefined() || !lineValue.isUndefined()) {
            if (!linesValue.isUndefined()) {
                collectEntry(linesValue, {});
            }
            if (!lineValue.isUndefined()) {
                collectEntry(lineValue, {});
            }
        } else {
            for (auto it = constellationsObject.begin(); it != constellationsObject.end(); ++it) {
                collectEntry(it.value(), it.key());
            }
        }
    }

    return labelRefs;
}

}  // namespace skygate::ephemeris
