#pragma once

#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include "SkyHitTargetIndex.hpp"
#include "SkyObjectTrailBuilder.hpp"
#include "SkyRenderBuilders.hpp"
#include "SkySceneFramePipeline.hpp"
#include "SkySelectionOverlayBuilder.hpp"

#include "skygate/core/PreparedProjection.hpp"
#include "skygate/ephemeris/Types.hpp"

#include <cstdint>
#include <optional>
#include <span>

class SkyContextController;
class SkyTimeController;

class SkySceneModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(
        QObject* skyContextController
        READ skyContextController
        WRITE setSkyContextController
        NOTIFY skyContextControllerChanged
    )
    Q_PROPERTY(QVariantList overlayItems READ overlayItems NOTIFY sceneFrameChanged)
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
    [[nodiscard]] QVariantMap selectionMarker() const;
    [[nodiscard]] QVariantMap selectedObjectInspector() const;
    [[nodiscard]] std::uint64_t snapshotGeneration() const noexcept;
    void setViewportSize(double viewportWidth, double viewportHeight);

    Q_INVOKABLE QString objectLabelAt(double x, double y) const;
    Q_INVOKABLE bool selectObjectAt(double x, double y);
    Q_INVOKABLE void clearSelectedObjectInspector();
    Q_INVOKABLE void setSelectedObjectInspectorPinned(bool pinned);
    Q_INVOKABLE void moveSelectedObjectInspector(double x, double y);

    [[nodiscard]] std::optional<skygate::core::PreparedProjection> preparedProjection() const;
    [[nodiscard]] std::span<const SkyRenderPoint> renderPointSpan() const;
    [[nodiscard]] std::span<const SkyRenderLine> renderLineSpan() const;
    [[nodiscard]] std::span<const SkyRenderGlyph> renderGlyphSpan() const;

signals:
    void skyContextControllerChanged();
    void sceneFrameChanged();

private:
    struct SceneCompositionKey final {
        std::uint64_t renderFrameGeneration = 0;
        std::optional<std::uint32_t> trailTargetBodyIndex;
        QString selectedObjectTargetId;
        QString selectedSearchTargetKind;
        QString selectedSearchTargetId;
        QString trackedTargetKind;
        QString trackedTargetId;
        double inspectorPinnedX = 0.0;
        double inspectorPinnedY = 0.0;
        bool inspectorPinned = false;

        [[nodiscard]] bool equals(const SceneCompositionKey& other) const noexcept
        {
            return renderFrameGeneration == other.renderFrameGeneration
                && trailTargetBodyIndex == other.trailTargetBodyIndex
                && selectedObjectTargetId == other.selectedObjectTargetId
                && selectedSearchTargetKind == other.selectedSearchTargetKind
                && selectedSearchTargetId == other.selectedSearchTargetId
                && trackedTargetKind == other.trackedTargetKind
                && trackedTargetId == other.trackedTargetId
                && inspectorPinnedX == other.inspectorPinnedX
                && inspectorPinnedY == other.inspectorPinnedY
                && inspectorPinned == other.inspectorPinned;
        }
    };

    struct SceneFrameData final {
        std::optional<skygate::core::PreparedProjection> preparedProjection;
        const skygate::ephemeris::SkySnapshot* snapshot = nullptr;
        SkyRenderFrame frame;
        QVariantList overlayItems;
        QVariantMap selectionMarker;
        QVariantMap selectedObjectInspector;
    };

    struct SceneBuildInput final {
        SkySceneFramePipelineInput frameInput;
        std::span<const std::uint8_t> catalogSourceIds;
        QStringList catalogSourceLabels;
        const SkyTimeController* timeController = nullptr;
        QString selectedSearchTargetKind;
        QString selectedSearchTargetId;
        QString trackedTargetKind;
        QString trackedTargetId;
    };

private:
    void disconnectFromContextController();
    void rebuildSceneFrame();
    [[nodiscard]] bool clearSceneFrame();
    [[nodiscard]] std::optional<SceneBuildInput> buildSceneInput() const;
    [[nodiscard]] QVariantList buildOverlayItems(
        const SceneFrameData& sceneFrame,
        const SceneBuildInput& input
    ) const;
    [[nodiscard]] SkySelectionOverlayInput buildSelectionInput(
        const SceneBuildInput& input,
        const SkySceneFramePipelineResult& frameResult
    ) const;

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
    SkySelectionOverlayBuilder m_selectionOverlayBuilder;
    SkyObjectTrailBuilder m_objectTrailBuilder;
    std::optional<SceneCompositionKey> m_sceneCompositionKey;
    SceneFrameData m_sceneFrame;
    QString m_selectedObjectTargetId;
    double m_selectedObjectInspectorPinnedX = 0.0;
    double m_selectedObjectInspectorPinnedY = 0.0;
    bool m_selectedObjectInspectorPinned = false;
};
