#include "SkyOverlayLayerSettings.hpp"

SkyOverlayLayerSettings::SkyOverlayLayerSettings(QObject* parent)
    : QObject(parent)
{
}

bool SkyOverlayLayerSettings::horizon() const noexcept
{
    return m_visibility.horizon;
}

bool SkyOverlayLayerSettings::altAzGrid() const noexcept
{
    return m_visibility.altAzGrid;
}

bool SkyOverlayLayerSettings::constellationLines() const noexcept
{
    return m_visibility.constellationLines;
}

bool SkyOverlayLayerSettings::constellationLabels() const noexcept
{
    return m_visibility.constellationLabels;
}

bool SkyOverlayLayerSettings::ecliptic() const noexcept
{
    return m_visibility.ecliptic;
}

bool SkyOverlayLayerSettings::celestialEquator() const noexcept
{
    return m_visibility.celestialEquator;
}

bool SkyOverlayLayerSettings::meridian() const noexcept
{
    return m_visibility.meridian;
}

bool SkyOverlayLayerSettings::circumpolarBoundary() const noexcept
{
    return m_visibility.circumpolarBoundary;
}

bool SkyOverlayLayerSettings::solarSystemLabels() const noexcept
{
    return m_visibility.solarSystemLabels;
}

const SkyOverlayLayerVisibility& SkyOverlayLayerSettings::visibility() const noexcept
{
    return m_visibility;
}

void SkyOverlayLayerSettings::setVisibility(const SkyOverlayLayerVisibility& visibility)
{
    const SkyOverlayLayerVisibility previous = m_visibility;
    if (previous.equals(visibility)) {
        return;
    }

    m_visibility = visibility;
    if (previous.horizon != m_visibility.horizon) {
        emit horizonChanged();
    }
    if (previous.altAzGrid != m_visibility.altAzGrid) {
        emit altAzGridChanged();
    }
    if (previous.constellationLines != m_visibility.constellationLines) {
        emit constellationLinesChanged();
    }
    if (previous.constellationLabels != m_visibility.constellationLabels) {
        emit constellationLabelsChanged();
    }
    if (previous.ecliptic != m_visibility.ecliptic) {
        emit eclipticChanged();
    }
    if (previous.celestialEquator != m_visibility.celestialEquator) {
        emit celestialEquatorChanged();
    }
    if (previous.meridian != m_visibility.meridian) {
        emit meridianChanged();
    }
    if (previous.circumpolarBoundary != m_visibility.circumpolarBoundary) {
        emit circumpolarBoundaryChanged();
    }
    if (previous.solarSystemLabels != m_visibility.solarSystemLabels) {
        emit solarSystemLabelsChanged();
    }
    emit visibilityChanged();
}

void SkyOverlayLayerSettings::setHorizon(const bool horizon)
{
    SkyOverlayLayerVisibility visibility = m_visibility;
    visibility.horizon = horizon;
    setVisibility(visibility);
}

void SkyOverlayLayerSettings::setAltAzGrid(const bool altAzGrid)
{
    SkyOverlayLayerVisibility visibility = m_visibility;
    visibility.altAzGrid = altAzGrid;
    setVisibility(visibility);
}

void SkyOverlayLayerSettings::setConstellationLines(const bool constellationLines)
{
    SkyOverlayLayerVisibility visibility = m_visibility;
    visibility.constellationLines = constellationLines;
    setVisibility(visibility);
}

void SkyOverlayLayerSettings::setConstellationLabels(const bool constellationLabels)
{
    SkyOverlayLayerVisibility visibility = m_visibility;
    visibility.constellationLabels = constellationLabels;
    setVisibility(visibility);
}

void SkyOverlayLayerSettings::setEcliptic(const bool ecliptic)
{
    SkyOverlayLayerVisibility visibility = m_visibility;
    visibility.ecliptic = ecliptic;
    setVisibility(visibility);
}

void SkyOverlayLayerSettings::setCelestialEquator(const bool celestialEquator)
{
    SkyOverlayLayerVisibility visibility = m_visibility;
    visibility.celestialEquator = celestialEquator;
    setVisibility(visibility);
}

void SkyOverlayLayerSettings::setMeridian(const bool meridian)
{
    SkyOverlayLayerVisibility visibility = m_visibility;
    visibility.meridian = meridian;
    setVisibility(visibility);
}

void SkyOverlayLayerSettings::setCircumpolarBoundary(const bool circumpolarBoundary)
{
    SkyOverlayLayerVisibility visibility = m_visibility;
    visibility.circumpolarBoundary = circumpolarBoundary;
    setVisibility(visibility);
}

void SkyOverlayLayerSettings::setSolarSystemLabels(const bool solarSystemLabels)
{
    SkyOverlayLayerVisibility visibility = m_visibility;
    visibility.solarSystemLabels = solarSystemLabels;
    setVisibility(visibility);
}
