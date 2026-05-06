#pragma once

#include <QColor>
#include <QString>

#include <vector>

struct SkyOverlayItem final {
    QString kind;
    double x = 0.0;
    double y = 0.0;
    QString text;
    QColor color;
};

struct SkySelectionMarker final {
    bool visible = false;
    QString kind;
    double x = 0.0;
    double y = 0.0;
};

struct SkyInspectorField final {
    QString label;
    QString value;
};

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
