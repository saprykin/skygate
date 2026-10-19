#pragma once

#include "ObservationContext.hpp"
#include "PreparedProjection.hpp"
#include "SkyInspectorPlacement.hpp"
#include "SkySceneComposer.hpp"
#include "SkySceneCompositionInput.hpp"
#include "SkySceneDegradationReporter.hpp"
#include "SkySceneFrameData.hpp"
#include "SkySceneFramePipeline.hpp"
#include "SkySceneOverlayAdapter.hpp"
#include "SkyViewportSnapshot.hpp"
#include "SkyViewportSnapshotPublisher.hpp"
#include "render/SkyRenderGlyph.hpp"
#include "render/SkyRenderLine.hpp"
#include "render/SkyRenderPoint.hpp"
#include "selection/SkyHitTargetIndex.hpp"

#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <cstdint>
#include <memory>
#include <optional>
#include <span>

class SkyContextController;

class SkySceneModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(
        QObject* skyContextController
        READ skyContextController
        WRITE setSkyContextController
        NOTIFY skyContextControllerChanged
    )
    Q_PROPERTY(QVariantList overlayItems READ overlayItems NOTIFY sceneFrameChanged)
    Q_PROPERTY(QVariantList ephemerisDegradationReasons READ ephemerisDegradationReasons NOTIFY sceneFrameChanged)
    Q_PROPERTY(QVariantMap selectionMarker READ selectionMarker NOTIFY sceneFrameChanged)
    Q_PROPERTY(
        QVariantMap selectedObjectInspector
        READ selectedObjectInspector
        NOTIFY sceneFrameChanged
    )

public:
    explicit SkySceneModel(QObject* parent = nullptr);

    [[nodiscard]] QObject* skyContextController() const noexcept;
    void setSkyContextController(QObject* skyContextController);

    [[nodiscard]] QVariantList overlayItems() const;
    [[nodiscard]] QVariantList ephemerisDegradationReasons() const;
    [[nodiscard]] QVariantMap selectionMarker() const;
    [[nodiscard]] QVariantMap selectedObjectInspector() const;
    [[nodiscard]] std::uint64_t snapshotGeneration() const noexcept;
    [[nodiscard]] std::shared_ptr<const SkyViewportSnapshot> viewportSnapshot() const noexcept;
    void setViewportSize(double viewportWidth, double viewportHeight);

    Q_INVOKABLE QString objectLabelAt(double x, double y) const;
    Q_INVOKABLE bool selectObjectAt(double x, double y);
    Q_INVOKABLE void clearSelectedObjectInspector();
    Q_INVOKABLE void setSelectedObjectInspectorPinned(bool pinned);
    Q_INVOKABLE void moveSelectedObjectInspector(double x, double y);

    [[nodiscard]] std::optional<skygate::core::PreparedProjection> preparedProjection() const;
    [[nodiscard]] std::optional<skygate::core::ObservationContext> referenceOverlayContext() const;
    [[nodiscard]] std::span<const SkyRenderPoint> renderPointSpan() const;
    [[nodiscard]] std::span<const SkyRenderLine> renderLineSpan() const;
    [[nodiscard]] std::span<const SkyRenderGlyph> renderGlyphSpan() const;

signals:
    void skyContextControllerChanged();
    void sceneFrameChanged();
    void viewportSnapshotChanged();

private:
    void disconnectFromContextController();
    void rebuildSceneFrame();
    [[nodiscard]] bool clearSceneFrame();
    [[nodiscard]] std::optional<SkySceneCompositionInput> buildSceneInput() const;
    void publishViewportSnapshot(
        std::uint64_t renderFrameGeneration, const std::optional<std::uint32_t>& trailTargetBodyIndex
    );

private:
    QPointer<SkyContextController> m_skyContextController;
    QMetaObject::Connection m_skyContextChangedConnection;
    QMetaObject::Connection m_selectedSearchTargetChangedConnection;
    QMetaObject::Connection m_trackedTargetChangedConnection;
    QMetaObject::Connection m_themeChangedConnection;
    QMetaObject::Connection m_timeZoneChangedConnection;
    double m_viewportWidth = 0.0;
    double m_viewportHeight = 0.0;
    SkySceneFramePipeline m_framePipeline;
    SkyHitTargetIndex m_hitTargetIndex;
    SkySceneComposer m_sceneComposer;
    SkySceneOverlayAdapter m_sceneOverlayAdapter;
    SkySceneFrameData m_sceneFrame;
    SkySceneDegradationReporter m_degradationReporter;
    SkyViewportSnapshotPublisher m_viewportSnapshotPublisher;
    SkyInspectorPlacement m_inspectorPlacement;
    QVariantList m_overlayItems;
    QVariantMap m_selectionMarker;
    QVariantMap m_selectedObjectInspector;
    bool m_ignoringSearchTargetChange = false;
};
