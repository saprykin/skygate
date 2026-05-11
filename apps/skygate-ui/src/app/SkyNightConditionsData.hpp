#pragma once

#include <QString>

#include <vector>

struct SkyNightConditionSunRow final {
    QString label;
    QString value;
};

struct SkyNightConditionsData final {
    bool valid = false;
    QString locationText;
    std::vector<SkyNightConditionSunRow> sunRows;
    QString moonPhaseText;
    QString moonRiseText;
    QString moonSetText;
};
