#pragma once

#include "SkyNightConditionSunRow.hpp"

#include <QString>

#include <vector>

struct SkyNightConditionsData final {
    bool valid = false;
    QString locationText;
    std::vector<SkyNightConditionSunRow> sunRows;
    QString moonPhaseText;
    QString moonRiseText;
    QString moonSetText;
};
