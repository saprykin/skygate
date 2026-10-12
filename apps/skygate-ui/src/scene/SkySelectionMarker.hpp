#pragma once

#include <QString>

struct SkySelectionMarker final {
    bool visible = false;
    QString kind;
    double x = 0.0;
    double y = 0.0;
};
