#include "SkySceneOverlayAdapter.hpp"
#include "SkyOverlayTestSupport.hpp"

#include <QtTest/QtTest>

#include <vector>

using skygate::ui::tests::verifyOverlayItemPayload;
using skygate::ui::tests::verifySelectedObjectInspectorPayload;
using skygate::ui::tests::verifySelectionMarkerPayload;

class SkySceneOverlayAdapterTests final : public QObject {
    Q_OBJECT

private slots:
    void overlayItemsMapTypedItemsInOrder();
    void hiddenSelectionPayloadsStayEmpty();
    void selectionMarkerMapsVisibleMarker();
    void selectedObjectInspectorMapsFieldsAndAliases();
};

void SkySceneOverlayAdapterTests::overlayItemsMapTypedItemsInOrder()
{
    const SkySceneOverlayAdapter adapter;
    const std::vector<SkyOverlayItem> overlayItems = {
        {
            .kind = QStringLiteral("label"),
            .x = 120.5,
            .y = 160.25,
            .text = QStringLiteral("Vega"),
            .color = QColor(QStringLiteral("#c9dcff"))
        },
        {
            .x = 450.0,
            .y = 40.0,
            .text = QStringLiteral("N"),
            .color = QColor(QStringLiteral("#ffcc66"))
        }
    };

    const QVariantList payload = adapter.overlayItems(overlayItems);

    QCOMPARE(payload.size(), static_cast<qsizetype>(overlayItems.size()));
    verifyOverlayItemPayload(payload.at(0), overlayItems.at(0));
    verifyOverlayItemPayload(payload.at(1), overlayItems.at(1));
}

void SkySceneOverlayAdapterTests::hiddenSelectionPayloadsStayEmpty()
{
    const SkySceneOverlayAdapter adapter;

    verifySelectionMarkerPayload(adapter.selectionMarker({}), {});
    verifySelectedObjectInspectorPayload(adapter.selectedObjectInspector({}), {});
}

void SkySceneOverlayAdapterTests::selectionMarkerMapsVisibleMarker()
{
    const SkySceneOverlayAdapter adapter;
    const SkySelectionMarker marker {
        .visible = true,
        .kind = QStringLiteral("searchSelection"),
        .x = 220.0,
        .y = 210.0
    };

    verifySelectionMarkerPayload(adapter.selectionMarker(marker), marker);
}

void SkySceneOverlayAdapterTests::selectedObjectInspectorMapsFieldsAndAliases()
{
    const SkySceneOverlayAdapter adapter;
    const SkySelectedObjectInspector inspector {
        .visible = true,
        .x = 260.0,
        .y = 180.0,
        .targetKind = QStringLiteral("body"),
        .targetId = QStringLiteral("messier_031"),
        .title = QStringLiteral("M31"),
        .pinned = true,
        .fields = {
            {.label = QStringLiteral("Type"), .value = QStringLiteral("Galaxy")},
            {.label = QStringLiteral("Rise"), .value = QStringLiteral("Always above")}
        },
        .aliases = QStringLiteral("Andromeda Galaxy")
    };

    verifySelectedObjectInspectorPayload(
        adapter.selectedObjectInspector(inspector),
        inspector
    );
}

QTEST_APPLESS_MAIN(SkySceneOverlayAdapterTests)

#include "SkySceneOverlayAdapterTests.moc"
