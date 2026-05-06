#pragma once

#include "SkySceneOverlayData.hpp"

#include <QtTest/QtTest>

#include <QVariantList>
#include <QVariantMap>

#include <cstddef>

namespace skygate::ui::tests {

[[nodiscard]] inline QString overlayInspectorFieldValue(
    const SkySelectedObjectInspector& inspector,
    const QString& label
)
{
    for (const SkyInspectorField& field : inspector.fields) {
        if (field.label == label) {
            return field.value;
        }
    }
    return {};
}

inline void verifyOverlayItemPayload(
    const QVariant& payload,
    const SkyOverlayItem& expected
)
{
    const QVariantMap map = payload.toMap();
    QVERIFY2(!map.isEmpty(), "overlay item payload must be a QVariantMap");
    if (expected.kind.isEmpty()) {
        QVERIFY(!map.contains(QStringLiteral("kind")));
    } else {
        QCOMPARE(map.value(QStringLiteral("kind")).toString(), expected.kind);
    }
    QCOMPARE(map.value(QStringLiteral("x")).toDouble(), expected.x);
    QCOMPARE(map.value(QStringLiteral("y")).toDouble(), expected.y);
    QCOMPARE(map.value(QStringLiteral("text")).toString(), expected.text);
    QCOMPARE(map.value(QStringLiteral("color")).value<QColor>(), expected.color);
}

inline void verifySelectionMarkerPayload(
    const QVariantMap& payload,
    const SkySelectionMarker& expected
)
{
    if (!expected.visible) {
        QVERIFY(payload.isEmpty());
        return;
    }

    QVERIFY2(!payload.isEmpty(), "visible selection marker must produce a payload");
    QCOMPARE(payload.value(QStringLiteral("kind")).toString(), expected.kind);
    QCOMPARE(payload.value(QStringLiteral("x")).toDouble(), expected.x);
    QCOMPARE(payload.value(QStringLiteral("y")).toDouble(), expected.y);
}

inline void verifySelectedObjectInspectorPayload(
    const QVariantMap& payload,
    const SkySelectedObjectInspector& expected
)
{
    if (!expected.visible) {
        QVERIFY(payload.isEmpty());
        return;
    }

    QVERIFY2(!payload.isEmpty(), "visible object inspector must produce a payload");
    QCOMPARE(payload.value(QStringLiteral("visible")).toBool(), true);
    QCOMPARE(payload.value(QStringLiteral("x")).toDouble(), expected.x);
    QCOMPARE(payload.value(QStringLiteral("y")).toDouble(), expected.y);
    QCOMPARE(payload.value(QStringLiteral("targetKind")).toString(), expected.targetKind);
    QCOMPARE(payload.value(QStringLiteral("targetId")).toString(), expected.targetId);
    QCOMPARE(payload.value(QStringLiteral("title")).toString(), expected.title);
    QCOMPARE(payload.value(QStringLiteral("pinned")).toBool(), expected.pinned);
    QCOMPARE(payload.value(QStringLiteral("aliases")).toString(), expected.aliases);

    const QVariantList fields = payload.value(QStringLiteral("fields")).toList();
    QCOMPARE(fields.size(), static_cast<qsizetype>(expected.fields.size()));
    for (qsizetype index = 0; index < fields.size(); ++index) {
        const QVariantMap fieldMap = fields.at(index).toMap();
        QVERIFY2(!fieldMap.isEmpty(), "inspector field payload must be a QVariantMap");
        const SkyInspectorField& expectedField = expected.fields.at(
            static_cast<std::size_t>(index)
        );
        QCOMPARE(fieldMap.value(QStringLiteral("label")).toString(), expectedField.label);
        QCOMPARE(fieldMap.value(QStringLiteral("value")).toString(), expectedField.value);
    }
}

}  // namespace skygate::ui::tests
