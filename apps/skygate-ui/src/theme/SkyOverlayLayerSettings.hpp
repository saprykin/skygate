#pragma once

#include <QObject>

#include "SkyOverlayLayerVisibility.hpp"

class SkyOverlayLayerSettings final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool horizon READ horizon WRITE setHorizon NOTIFY horizonChanged)
    Q_PROPERTY(bool altAzGrid READ altAzGrid WRITE setAltAzGrid NOTIFY altAzGridChanged)
    Q_PROPERTY(
        bool constellationLines
        READ constellationLines
        WRITE setConstellationLines
        NOTIFY constellationLinesChanged
    )
    Q_PROPERTY(
        bool constellationLabels
        READ constellationLabels
        WRITE setConstellationLabels
        NOTIFY constellationLabelsChanged
    )
    Q_PROPERTY(bool ecliptic READ ecliptic WRITE setEcliptic NOTIFY eclipticChanged)
    Q_PROPERTY(
        bool celestialEquator
        READ celestialEquator
        WRITE setCelestialEquator
        NOTIFY celestialEquatorChanged
    )
    Q_PROPERTY(
        bool circumpolarBoundary
        READ circumpolarBoundary
        WRITE setCircumpolarBoundary
        NOTIFY circumpolarBoundaryChanged
    )
    Q_PROPERTY(
        bool solarSystemLabels
        READ solarSystemLabels
        WRITE setSolarSystemLabels
        NOTIFY solarSystemLabelsChanged
    )
    Q_PROPERTY(
        bool deepSkyObjects
        READ deepSkyObjects
        WRITE setDeepSkyObjects
        NOTIFY deepSkyObjectsChanged
    )
    Q_PROPERTY(
        bool deepSkyLabels
        READ deepSkyLabels
        WRITE setDeepSkyLabels
        NOTIFY deepSkyLabelsChanged
    )

public:
    explicit SkyOverlayLayerSettings(QObject* parent = nullptr);

    [[nodiscard]] bool horizon() const noexcept;
    [[nodiscard]] bool altAzGrid() const noexcept;
    [[nodiscard]] bool constellationLines() const noexcept;
    [[nodiscard]] bool constellationLabels() const noexcept;
    [[nodiscard]] bool ecliptic() const noexcept;
    [[nodiscard]] bool celestialEquator() const noexcept;
    [[nodiscard]] bool circumpolarBoundary() const noexcept;
    [[nodiscard]] bool solarSystemLabels() const noexcept;
    [[nodiscard]] bool deepSkyObjects() const noexcept;
    [[nodiscard]] bool deepSkyLabels() const noexcept;
    [[nodiscard]] const SkyOverlayLayerVisibility& visibility() const noexcept;

    void setVisibility(const SkyOverlayLayerVisibility& visibility);

public slots:
    void setHorizon(bool horizon);
    void setAltAzGrid(bool altAzGrid);
    void setConstellationLines(bool constellationLines);
    void setConstellationLabels(bool constellationLabels);
    void setEcliptic(bool ecliptic);
    void setCelestialEquator(bool celestialEquator);
    void setCircumpolarBoundary(bool circumpolarBoundary);
    void setSolarSystemLabels(bool solarSystemLabels);
    void setDeepSkyObjects(bool deepSkyObjects);
    void setDeepSkyLabels(bool deepSkyLabels);

signals:
    void horizonChanged();
    void altAzGridChanged();
    void constellationLinesChanged();
    void constellationLabelsChanged();
    void eclipticChanged();
    void celestialEquatorChanged();
    void circumpolarBoundaryChanged();
    void solarSystemLabelsChanged();
    void deepSkyObjectsChanged();
    void deepSkyLabelsChanged();
    void visibilityChanged();

private:
    SkyOverlayLayerVisibility m_visibility;
};
