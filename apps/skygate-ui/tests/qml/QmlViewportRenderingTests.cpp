#include "QmlRenderingTestSupport.hpp"

class QmlViewportRenderingTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void skyViewportItemRendersNonBlankScene();
    void skyViewportItemHandlesModelLifecycleGeometryAndFrameChanges();
    void skyViewportItemOverlayLayersAddExpectedGeometry_data();
    void skyViewportItemOverlayLayersAddExpectedGeometry();
    void skyViewportItemAstronomicalLayersAddExpectedGeometry_data();
    void skyViewportItemAstronomicalLayersAddExpectedGeometry();

private:
    QmlSettingsFixture m_settings;
};

void QmlViewportRenderingTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("QmlViewportRenderingTests")));
}

void QmlViewportRenderingTests::init()
{
    m_settings.resetForCurrentTest();
}

void QmlViewportRenderingTests::skyViewportItemRendersNonBlankScene()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);
    controller->setLive(false);
    QVERIFY(controller->setUtcDateTimeText(QStringLiteral("2024-09-01"), QStringLiteral("22:00:00")));
    controller->setLatitudeText(QStringLiteral("47.3769"));
    controller->setLongitudeText(QStringLiteral("8.5417"));
    controller->setElevationText(QStringLiteral("408.0"));
    controller->setMagnitudeCutoff(8.0);

    const auto snapshot = controller->ephemerisEngine()->compute(controller->skyContext());
    const auto m31StateIt = std::find_if(
        snapshot.states.begin(),
        snapshot.states.end(),
        [&snapshot](const skygate::ephemeris::CelestialBodyState& state) {
            return snapshot.bodyAt(state.bodyIndex).id == "messier_031";
        }
    );
    QVERIFY(m31StateIt != snapshot.states.end());
    controller->setViewCenter(m31StateIt->horizontal.altitudeDeg, m31StateIt->horizontal.azimuthDeg);

    auto sceneModel = makeSceneModel(*controller);
    QVERIFY(sceneModel != nullptr);

    TestSkyViewportItem viewport;
    viewport.setWidth(640.0);
    viewport.setHeight(420.0);
    viewport.setSkySceneModel(sceneModel.get());
    QTRY_VERIFY(sceneModel->snapshotGeneration() > 0U);
    QTRY_VERIFY(!sceneModel->renderPointSpan().empty() || !sceneModel->renderLineSpan().empty());

    std::unique_ptr<QSGNode> paintNode(viewport.buildPaintNode());
    QVERIFY(paintNode != nullptr);
    QVERIFY(nodeTreeSize(paintNode.get()) > 3);
    const SkyViewGeometryProbe sceneProbe = geometryProbe(paintNode.get());
    QVERIFY2(
        geometryProbeIntersectsViewport(sceneProbe, viewport.size()),
        qPrintable(QStringLiteral("Viewport geometry bounds [%1,%2 %3x%4] missed %5x%6")
            .arg(sceneProbe.bounds.x())
            .arg(sceneProbe.bounds.y())
            .arg(sceneProbe.bounds.width())
            .arg(sceneProbe.bounds.height())
            .arg(viewport.width())
            .arg(viewport.height()))
    );
    QString failure;
    QVERIFY2(
        renderingHasVisibleGeometryWithColor(
            paintNode.get(),
            controller->renderTheme().gridAzimuthLine,
            viewport.size(),
            &failure
        ),
        qPrintable(failure)
    );
}

void QmlViewportRenderingTests::skyViewportItemHandlesModelLifecycleGeometryAndFrameChanges()
{
    auto controller = makeController();
    QVERIFY(controller != nullptr);
    controller->setViewCenter(45.0, 180.0);
    auto sceneModel = makeSceneModel(*controller);
    QVERIFY(sceneModel != nullptr);

    TestSkyViewportItem viewport;
    QSignalSpy modelChangedSpy(&viewport, &SkyViewportItem::skySceneModelChanged);
    viewport.setWidth(640.0);
    viewport.setHeight(420.0);
    viewport.setSkySceneModel(sceneModel.get());
    QCOMPARE(modelChangedSpy.count(), 1);
    QCOMPARE(viewport.skySceneModel(), sceneModel.get());
    QTRY_VERIFY(sceneModel->preparedProjection().has_value());
    QCOMPARE(sceneModel->preparedProjection()->params().viewportWidth, 640.0);
    QCOMPARE(sceneModel->preparedProjection()->params().viewportHeight, 420.0);

    std::unique_ptr<QSGNode> paintNode(viewport.buildPaintNode());
    QVERIFY(paintNode != nullptr);
    const int populatedNodeCount = nodeTreeSize(paintNode.get());
    const int populatedVertexCount = geometryVertexCount(paintNode.get());
    QVERIFY(populatedNodeCount > 3);
    QVERIFY(populatedVertexCount > 0);
    QVERIFY(geometryProbeIntersectsViewport(geometryProbe(paintNode.get()), viewport.size()));

    viewport.setWidth(320.0);
    viewport.setHeight(240.0);
    QTRY_VERIFY(sceneModel->preparedProjection().has_value());
    QCOMPARE(sceneModel->preparedProjection()->params().viewportWidth, 320.0);
    QCOMPARE(sceneModel->preparedProjection()->params().viewportHeight, 240.0);
    paintNode.reset(viewport.buildPaintNode());
    QVERIFY(paintNode != nullptr);
    QVERIFY(geometryVertexCount(paintNode.get()) > 0);
    QVERIFY(geometryProbeIntersectsViewport(geometryProbe(paintNode.get()), viewport.size()));

    QObject* overlayLayers = controller->overlayLayers();
    QVERIFY(overlayLayers != nullptr);
    overlayLayers->setProperty("altAzGrid", false);
    QCoreApplication::processEvents();
    paintNode.reset(viewport.buildPaintNode());
    QVERIFY(paintNode != nullptr);
    const int changedVertexCount = geometryVertexCount(paintNode.get());
    QVERIFY(changedVertexCount > 0);
    QVERIFY(changedVertexCount != populatedVertexCount);

    viewport.setSkySceneModel(nullptr);
    QCOMPARE(modelChangedSpy.count(), 2);
    QCOMPARE(viewport.skySceneModel(), nullptr);
    paintNode.reset(viewport.buildPaintNode());
    QVERIFY(paintNode != nullptr);
    QCOMPARE(nodeTreeSize(paintNode.get()), 3);
    QCOMPARE(geometryVertexCount(paintNode.get()), 0);

    auto secondController = makeController();
    QVERIFY(secondController != nullptr);
    auto secondSceneModel = makeSceneModel(*secondController);
    QVERIFY(secondSceneModel != nullptr);
    viewport.setSkySceneModel(secondSceneModel.get());
    QCOMPARE(modelChangedSpy.count(), 3);
    QCOMPARE(viewport.skySceneModel(), secondSceneModel.get());
    QTRY_VERIFY(secondSceneModel->preparedProjection().has_value());
    QCOMPARE(secondSceneModel->preparedProjection()->params().viewportWidth, 320.0);
    QCOMPARE(secondSceneModel->preparedProjection()->params().viewportHeight, 240.0);
    paintNode.reset(viewport.buildPaintNode());
    QVERIFY(paintNode != nullptr);
    QVERIFY(geometryVertexCount(paintNode.get()) > 0);
}

void QmlViewportRenderingTests::skyViewportItemOverlayLayersAddExpectedGeometry_data()
{
    QTest::addColumn<QString>("propertyName");

    QTest::newRow("horizon") << QStringLiteral("horizon");
    QTest::newRow("alt-az-grid") << QStringLiteral("altAzGrid");
    QTest::newRow("celestial-equator") << QStringLiteral("celestialEquator");
    QTest::newRow("deep-sky-glyphs") << QStringLiteral("deepSkyObjects");
}

void QmlViewportRenderingTests::skyViewportItemOverlayLayersAddExpectedGeometry()
{
    QFETCH(QString, propertyName);

    auto controller = makeController();
    QVERIFY(controller != nullptr);
    controller->setMagnitudeCutoff(8.0);
    controller->setViewCenter(45.0, 180.0);

    if (propertyName == QStringLiteral("deepSkyObjects")) {
        controller->setLive(false);
        QVERIFY(controller->setUtcDateTimeText(QStringLiteral("2024-09-01"), QStringLiteral("22:00:00")));
        controller->setLatitudeText(QStringLiteral("47.3769"));
        controller->setLongitudeText(QStringLiteral("8.5417"));
        controller->setElevationText(QStringLiteral("408.0"));

        const auto snapshot = controller->ephemerisEngine()->compute(controller->skyContext());
        const auto m31StateIt = std::find_if(
            snapshot.states.begin(),
            snapshot.states.end(),
            [&snapshot](const skygate::ephemeris::CelestialBodyState& state) {
                return snapshot.bodyAt(state.bodyIndex).id == "messier_031";
            }
        );
        QVERIFY(m31StateIt != snapshot.states.end());
        controller->setViewCenter(m31StateIt->horizontal.altitudeDeg, m31StateIt->horizontal.azimuthDeg);
    }

    auto sceneModel = makeSceneModel(*controller);
    QVERIFY(sceneModel != nullptr);

    QObject* overlayLayers = controller->overlayLayers();
    QVERIFY(overlayLayers != nullptr);
    for (const char* layerProperty : {
        "horizon",
        "altAzGrid",
        "ecliptic",
        "celestialEquator",
        "circumpolarBoundary",
        "deepSkyObjects",
    }) {
        QVERIFY(overlayLayers->setProperty(layerProperty, false));
    }
    QCoreApplication::processEvents();

    TestSkyViewportItem viewport;
    viewport.setWidth(760.0);
    viewport.setHeight(520.0);
    viewport.setSkySceneModel(sceneModel.get());
    QTRY_VERIFY(sceneModel->preparedProjection().has_value());

    std::unique_ptr<QSGNode> paintNode(viewport.buildPaintNode());
    QVERIFY(paintNode != nullptr);
    const int baselineVertexCount = geometryVertexCount(paintNode.get());
    const QList<QColor> expectedColors = renderingExpectedGeometryColors(
        controller->renderTheme(),
        propertyName
    );
    QVERIFY(!expectedColors.isEmpty());
    for (const QColor& color : expectedColors) {
        QCOMPARE(geometryMaterialColorProbe(paintNode.get(), color).vertexCount, 0);
    }

    const QByteArray layerPropertyName = propertyName.toUtf8();
    QVERIFY(overlayLayers->setProperty(layerPropertyName.constData(), true));
    QCoreApplication::processEvents();
    viewport.setWidth(761.0);
    viewport.setWidth(760.0);
    paintNode.reset(viewport.buildPaintNode());
    QVERIFY(paintNode != nullptr);
    const int enabledVertexCount = geometryVertexCount(paintNode.get());

    QVERIFY2(
        enabledVertexCount > baselineVertexCount,
        qPrintable(QString("%1 did not add scene-graph geometry").arg(propertyName))
    );
    for (const QColor& color : expectedColors) {
        QString failure;
        QVERIFY2(
            renderingHasVisibleGeometryWithColor(
                paintNode.get(),
                color,
                viewport.size(),
                &failure
            ),
            qPrintable(QStringLiteral("%1: %2").arg(propertyName, failure))
        );
    }
}

void QmlViewportRenderingTests::skyViewportItemAstronomicalLayersAddExpectedGeometry_data()
{
    QTest::addColumn<QString>("propertyName");
    QTest::addColumn<QString>("labelText");
    QTest::addColumn<double>("latitudeDeg");
    QTest::addColumn<double>("longitudeDeg");
    QTest::addColumn<QDateTime>("utcTime");
    QTest::addColumn<double>("centerAltitudeDeg");
    QTest::addColumn<double>("centerAzimuthDeg");

    QTest::newRow("ecliptic-greenwich-equinox")
        << QStringLiteral("ecliptic")
        << QStringLiteral("Ecliptic")
        << 51.4769
        << 0.0
        << QDateTime(QDate(2026, 3, 20), QTime(10, 0), QTimeZone::UTC)
        << 30.0
        << 180.0;
    QTest::newRow("circumpolar-reykjavik")
        << QStringLiteral("circumpolarBoundary")
        << QStringLiteral("Circumpolar")
        << 64.1466
        << -21.9426
        << QDateTime(QDate(2026, 1, 15), QTime(0, 0), QTimeZone::UTC)
        << 45.0
        << 0.0;
}

void QmlViewportRenderingTests::skyViewportItemAstronomicalLayersAddExpectedGeometry()
{
    QFETCH(QString, propertyName);
    QFETCH(QString, labelText);
    QFETCH(double, latitudeDeg);
    QFETCH(double, longitudeDeg);
    QFETCH(QDateTime, utcTime);
    QFETCH(double, centerAltitudeDeg);
    QFETCH(double, centerAzimuthDeg);

    auto controller = makeController();
    QVERIFY(controller != nullptr);
    controller->setLatitudeText(QString::number(latitudeDeg, 'f', 4));
    controller->setLongitudeText(QString::number(longitudeDeg, 'f', 4));
    QVERIFY(controller->setUtcDateTimeText(
        utcTime.date().toString(QStringLiteral("yyyy-MM-dd")),
        utcTime.time().toString(QStringLiteral("HH:mm:ss"))
    ));
    controller->setViewCenter(centerAltitudeDeg, centerAzimuthDeg);
    auto sceneModel = makeSceneModel(*controller);
    QVERIFY(sceneModel != nullptr);

    QObject* overlayLayers = controller->overlayLayers();
    QVERIFY(overlayLayers != nullptr);
    for (const char* layerProperty : {
        "horizon",
        "altAzGrid",
        "ecliptic",
        "celestialEquator",
        "circumpolarBoundary",
        "deepSkyObjects",
    }) {
        QVERIFY(overlayLayers->setProperty(layerProperty, false));
    }
    QCoreApplication::processEvents();

    TestSkyViewportItem viewport;
    viewport.setWidth(760.0);
    viewport.setHeight(520.0);
    viewport.setSkySceneModel(sceneModel.get());
    QTRY_VERIFY(sceneModel->preparedProjection().has_value());

    std::unique_ptr<QSGNode> paintNode(viewport.buildPaintNode());
    QVERIFY(paintNode != nullptr);
    const int baselineVertexCount = geometryVertexCount(paintNode.get());
    const int baselineNodeCount = nodeTreeSize(paintNode.get());
    const int baselineOverlayItemCount = sceneModel->overlayItems().size();
    const QList<QColor> expectedColors = renderingExpectedGeometryColors(
        controller->renderTheme(),
        propertyName
    );
    QVERIFY(!expectedColors.isEmpty());
    for (const QColor& color : expectedColors) {
        QCOMPARE(geometryMaterialColorProbe(paintNode.get(), color).vertexCount, 0);
    }

    const QByteArray layerPropertyName = propertyName.toUtf8();
    QVERIFY(overlayLayers->setProperty(layerPropertyName.constData(), true));
    QCoreApplication::processEvents();
    viewport.setWidth(761.0);
    viewport.setWidth(760.0);
    paintNode.reset(viewport.buildPaintNode());
    QVERIFY(paintNode != nullptr);

    QVERIFY2(
        geometryVertexCount(paintNode.get()) > baselineVertexCount,
        qPrintable(QString("%1 did not add scene-graph geometry").arg(propertyName))
    );
    QVERIFY(nodeTreeSize(paintNode.get()) >= baselineNodeCount);
    QVERIFY(sceneModel->overlayItems().size() > baselineOverlayItemCount);
    for (const QColor& color : expectedColors) {
        QString failure;
        QVERIFY2(
            renderingHasVisibleGeometryWithColor(
                paintNode.get(),
                color,
                viewport.size(),
                &failure
            ),
            qPrintable(QStringLiteral("%1: %2").arg(propertyName, failure))
        );
    }

    bool hasReferenceLabel = false;
    for (const QVariant& item : sceneModel->overlayItems()) {
        const QVariantMap map = item.toMap();
        if (map.value(QStringLiteral("text")).toString() == labelText) {
            hasReferenceLabel = true;
            break;
        }
    }
    QVERIFY2(hasReferenceLabel, qPrintable(QString("%1 label was not projected").arg(labelText)));
}

SKYGATE_QML_TEST_MAIN(QmlViewportRenderingTests)

#include "QmlViewportRenderingTests.moc"
