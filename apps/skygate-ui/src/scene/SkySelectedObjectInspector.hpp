#pragma once

#include "SkyInspectorField.hpp"

#include <QString>

#include <vector>

struct SkySelectedObjectInspector final {
    bool visible = false;
    double x = 0.0;
    double y = 0.0;
    QString targetKind;
    QString targetId;
    QString title;
    bool pinned = false;
    std::vector<SkyInspectorField> fields;
    QString aliases;
};
