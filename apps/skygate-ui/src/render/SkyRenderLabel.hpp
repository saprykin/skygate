#pragma once

#include <QColor>
#include <QString>

struct SkyRenderLabel final {
    QString kind;
    double x = 0.0;
    double y = 0.0;
    QString text;
    QColor color;
};
