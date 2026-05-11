#pragma once

#include "SkyNightConditionsData.hpp"

#include <QVariantList>
#include <QVariantMap>

class SkyNightConditionsAdapter final {
public:
    [[nodiscard]] QVariantMap nightConditions(
        const SkyNightConditionsData& conditions
    ) const
    {
        QVariantList sunRows;
        sunRows.reserve(static_cast<qsizetype>(conditions.sunRows.size()));
        for (const SkyNightConditionSunRow& row : conditions.sunRows) {
            QVariantMap entry;
            entry.insert("label", row.label);
            entry.insert("value", row.value);
            sunRows.push_back(entry);
        }

        QVariantMap entry;
        entry.insert("valid", conditions.valid);
        entry.insert("locationText", conditions.locationText);
        entry.insert("sunRows", sunRows);
        entry.insert("moonPhaseText", conditions.moonPhaseText);
        entry.insert("moonRiseText", conditions.moonRiseText);
        entry.insert("moonSetText", conditions.moonSetText);
        return entry;
    }
};
