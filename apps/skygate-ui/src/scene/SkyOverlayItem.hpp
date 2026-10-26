#pragma once

#include <QColor>
#include <QString>

struct SkyOverlayItem final {
    QString kind;
    double x = 0.0;
    double y = 0.0;
    QString text;
    QColor color;
};
