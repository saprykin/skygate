#pragma once

#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QHash>
#include <QVariantMap>
#include <QVariantList>

#include "SkyRenderBuilders.hpp"

#include "skygate/core/PreparedProjection.hpp"
#include "skygate/core/ProjectionTypes.hpp"
#include "skygate/ephemeris/Types.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

class SkyContextController;
namespace skygate::ephemeris {
class IEphemerisEngine;
}

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

public:
    explicit SkySceneModel(QObject* parent = nullptr);

    [[nodiscard]] QObject* skyContextController() const noexcept;
    void setSkyContextController(QObject* skyContextController);

    [[nodiscard]] QVariantList overlayItems() const;
    [[nodiscard]] QVariantMap selectionMarker() const;
    [[nodiscard]] std::uint64_t snapshotGeneration() const noexcept;
    void setViewportSize(double viewportWidth, double viewportHeight);

    Q_INVOKABLE QString objectLabelAt(double x, double y) const;

    [[nodiscard]] std::optional<skygate::core::PreparedProjection> preparedProjection() const;
    [[nodiscard]] std::span<const SkyRenderPoint> renderPointSpan() const;
    [[nodiscard]] std::span<const SkyRenderLine> renderLineSpan() const;

signals:
    void skyContextControllerChanged();
    void sceneFrameChanged();

private:
    struct SnapshotCacheKey final {
        std::uint64_t catalogRevision = 0;
        skygate::core::GeoLocation observer;
        skygate::core::UtcTimePoint utcTime {};

        [[nodiscard]] bool equals(const SnapshotCacheKey& other) const noexcept
        {
            return catalogRevision == other.catalogRevision
                && observer.latitudeDeg == other.observer.latitudeDeg
                && observer.longitudeDeg == other.observer.longitudeDeg
                && observer.elevationMeters == other.observer.elevationMeters
                && utcTime == other.utcTime;
        }
    };

    struct RenderFrameKey final {
        std::uint64_t snapshotGeneration = 0;
        skygate::core::ProjectionType projectionType =
            skygate::core::ProjectionType::Stereographic;
        double viewportWidth = 0.0;
        double viewportHeight = 0.0;
        double viewCenterAltitudeDeg = 0.0;
        double viewCenterAzimuthDeg = 0.0;
        double viewFieldOfViewDeg = 0.0;
        double magnitudeCutoff = 0.0;
        QString themeId;
        SkyOverlayLayerVisibility overlayLayers;

        [[nodiscard]] bool equals(const RenderFrameKey& other) const noexcept
        {
            return snapshotGeneration == other.snapshotGeneration
                && projectionType == other.projectionType
                && viewportWidth == other.viewportWidth
                && viewportHeight == other.viewportHeight
                && viewCenterAltitudeDeg == other.viewCenterAltitudeDeg
                && viewCenterAzimuthDeg == other.viewCenterAzimuthDeg
                && viewFieldOfViewDeg == other.viewFieldOfViewDeg
                && magnitudeCutoff == other.magnitudeCutoff
                && themeId == other.themeId
                && overlayLayers.equals(other.overlayLayers);
        }
    };

    struct SceneFrameData final {
        std::optional<skygate::core::PreparedProjection> preparedProjection;
        skygate::ephemeris::SkySnapshot snapshot;
        SkyRenderFrame frame;
        std::unordered_map<std::uint64_t, std::vector<std::size_t>> hoverPointIndicesByCell;
        QHash<QString, std::size_t> stateIndexByBodyId;
        QVariantList overlayItems;
        QVariantMap selectionMarker;
    };

private:
    void disconnectFromContextController();
    void rebuildSceneFrame();
    [[nodiscard]] bool clearSceneFrame();
    [[nodiscard]] QVariantList buildOverlayItems(const SceneFrameData& sceneFrame) const;
    [[nodiscard]] QVariantMap buildSelectionMarker(const SceneFrameData& sceneFrame) const;

private:
    QPointer<SkyContextController> m_skyContextController;
    QMetaObject::Connection m_skyContextChangedConnection;
    QMetaObject::Connection m_selectedSearchTargetChangedConnection;
    QMetaObject::Connection m_themeChangedConnection;
    double m_viewportWidth = 0.0;
    double m_viewportHeight = 0.0;
    const skygate::ephemeris::IEphemerisEngine* m_cachedEphemerisEngine = nullptr;
    std::optional<SnapshotCacheKey> m_snapshotCacheKey;
    std::optional<RenderFrameKey> m_renderFrameKey;
    std::uint64_t m_snapshotGeneration = 0;
    SceneFrameData m_sceneFrame;
};
