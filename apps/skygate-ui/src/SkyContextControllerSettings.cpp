#include "SkyContextController.hpp"

#include "SkyContextControllerSupport.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>
#include <QTimeZone>

#include "skygate/ephemeris/StarCatalogFactory.hpp"

using namespace skygate::ui::internal;

namespace {
QString stripSavedSuffixes(const QString& sourceLabel)
{
    QString normalizedSourceLabel = sourceLabel.trimmed();
    const QString spacedSavedSuffix = QStringLiteral(" (saved)");
    const QString compactSavedSuffix = QStringLiteral("(saved)");
    while (
        normalizedSourceLabel.endsWith(spacedSavedSuffix, Qt::CaseInsensitive)
        || normalizedSourceLabel.endsWith(compactSavedSuffix, Qt::CaseInsensitive)
    ) {
        if (normalizedSourceLabel.endsWith(spacedSavedSuffix, Qt::CaseInsensitive)) {
            normalizedSourceLabel.chop(spacedSavedSuffix.size());
        } else {
            normalizedSourceLabel.chop(compactSavedSuffix.size());
        }
        normalizedSourceLabel = normalizedSourceLabel.trimmed();
    }
    return normalizedSourceLabel;
}
} // namespace

bool SkyContextController::saveSettings() const
{
    QSettings settings;
    settings.setValue(settingsKey("version"), kSettingsVersion);
    settings.setValue(settingsKey("live"), m_live);
    settings.setValue(settingsKey("speedMultiplier"), m_speedMultiplier);
    settings.setValue(settingsKey("stepSeconds"), m_stepSeconds);
    settings.setValue(settingsKey("magnitudeCutoff"), m_magnitudeCutoff);
    settings.setValue(settingsKey("viewCenterAltitudeDeg"), m_viewCenterAltitudeDeg);
    settings.setValue(settingsKey("viewCenterAzimuthDeg"), m_viewCenterAzimuthDeg);
    settings.setValue(settingsKey("viewFieldOfViewDeg"), m_viewFieldOfViewDeg);
    settings.setValue(settingsKey("utcEpochSeconds"), toQDateTimeUtc(m_skyContext.utcTime).toSecsSinceEpoch());
    settings.setValue(settingsKey("latitudeDeg"), m_skyContext.observer.latitudeDeg);
    settings.setValue(settingsKey("longitudeDeg"), m_skyContext.observer.longitudeDeg);
    settings.setValue(settingsKey("elevationMeters"), m_skyContext.observer.elevationMeters);
    settings.setValue(settingsKey("projectionType"), projectionTypeText());
    settings.setValue(settingsKey("catalogSourceLabel"), m_catalogSourceLabel);
    settings.setValue(settingsKey("catalogPresetIndex"), m_catalogPresetIndex);
    settings.setValue(settingsKey("catalogUrlText"), m_catalogUrlText);
    settings.sync();
    return settings.status() == QSettings::NoError;
}

bool SkyContextController::loadSettings()
{
    QSettings settings;
    const bool hasSavedContext = settings.contains(settingsKey("version"));
    if (hasSavedContext) {
        const bool live = settings.value(settingsKey("live"), m_live).toBool();
        const double speedMultiplier = settings.value(
            settingsKey("speedMultiplier"),
            m_speedMultiplier
        ).toDouble();
        const int stepSeconds = settings.value(
            settingsKey("stepSeconds"),
            m_stepSeconds
        ).toInt();
        const double magnitudeCutoff = settings.value(
            settingsKey("magnitudeCutoff"),
            m_magnitudeCutoff
        ).toDouble();
        const double viewCenterAltitudeDeg = settings.value(
            settingsKey("viewCenterAltitudeDeg"),
            m_viewCenterAltitudeDeg
        ).toDouble();
        const double viewCenterAzimuthDeg = settings.value(
            settingsKey("viewCenterAzimuthDeg"),
            m_viewCenterAzimuthDeg
        ).toDouble();
        const double viewFieldOfViewDeg = settings.value(
            settingsKey("viewFieldOfViewDeg"),
            m_viewFieldOfViewDeg
        ).toDouble();
        const qint64 utcEpochSeconds = settings.value(
            settingsKey("utcEpochSeconds"),
            toQDateTimeUtc(m_skyContext.utcTime).toSecsSinceEpoch()
        ).toLongLong();
        const double latitudeDeg = settings.value(
            settingsKey("latitudeDeg"),
            m_skyContext.observer.latitudeDeg
        ).toDouble();
        const double longitudeDeg = settings.value(
            settingsKey("longitudeDeg"),
            m_skyContext.observer.longitudeDeg
        ).toDouble();
        const double elevationMeters = settings.value(
            settingsKey("elevationMeters"),
            m_skyContext.observer.elevationMeters
        ).toDouble();
        const QString projectionType = settings.value(
            settingsKey("projectionType"),
            projectionTypeText()
        ).toString();
        const int catalogPresetIndex = settings.value(
            settingsKey("catalogPresetIndex"),
            m_catalogPresetIndex
        ).toInt();
        const QString catalogUrlText = settings.value(
            settingsKey("catalogUrlText"),
            m_catalogUrlText
        ).toString();

        setLive(live);
        setSpeedMultiplier(speedMultiplier);
        setStepSeconds(stepSeconds);
        setMagnitudeCutoff(magnitudeCutoff);
        setViewCenter(viewCenterAltitudeDeg, viewCenterAzimuthDeg);
        setViewFieldOfViewDeg(viewFieldOfViewDeg);
        setCurrentUtc(QDateTime::fromSecsSinceEpoch(utcEpochSeconds, QTimeZone::UTC));
        setLatitudeText(QString::number(latitudeDeg, 'f', 6));
        setLongitudeText(QString::number(longitudeDeg, 'f', 6));
        setElevationText(QString::number(elevationMeters, 'f', 1));
        setProjectionTypeText(projectionType);
        setCatalogPresetIndex(catalogPresetIndex);
        setCatalogUrlText(catalogUrlText);
    }

    restoreCatalogCache();
    return hasSavedContext;
}

void SkyContextController::persistCatalogCache(
    const std::vector<skygate::ephemeris::CelestialBody>& bodies,
    const QString& sourceLabel
) const
{
    if (bodies.empty()) {
        return;
    }

    QByteArray rows = serializeCatalogRows(bodies);
    if (rows.isEmpty()) {
        return;
    }

    QSettings settings;
    const QString configuredPath = settings.value(
        settingsKey("catalogCachePath"),
        defaultCatalogCachePath()
    ).toString();
    if (configuredPath.isEmpty()) {
        return;
    }

    const QFileInfo targetInfo(configuredPath);
    QDir targetDir(targetInfo.absolutePath());
    if (!targetDir.mkpath(".")) {
        return;
    }

    QSaveFile cacheFile(configuredPath);
    if (!cacheFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }

    const qint64 writtenBytes = cacheFile.write(rows);
    if (writtenBytes != rows.size()) {
        cacheFile.cancelWriting();
        return;
    }

    if (!cacheFile.commit()) {
        return;
    }

    settings.setValue(settingsKey("version"), kSettingsVersion);
    settings.setValue(settingsKey("catalogCachePath"), configuredPath);
    settings.setValue(settingsKey("catalogSourceLabel"), sourceLabel);
    settings.setValue(
        settingsKey("catalogConstellationLineRefs"),
        serializeConstellationLineRows(m_constellationLineRefs)
    );
    settings.sync();
}

void SkyContextController::restoreCatalogCache()
{
    QSettings settings;
    const QString configuredPath = settings.value(
        settingsKey("catalogCachePath"),
        defaultCatalogCachePath()
    ).toString();
    if (configuredPath.isEmpty()) {
        return;
    }

    QFile cacheFile(configuredPath);
    if (!cacheFile.exists() || !cacheFile.open(QIODevice::ReadOnly)) {
        return;
    }

    const QByteArray rows = cacheFile.readAll();
    if (rows.isEmpty()) {
        return;
    }

    std::unique_ptr<skygate::ephemeris::IStarCatalog> restoredCatalog =
        skygate::ephemeris::createStarCatalogFromRows(
            std::string_view(rows.constData(), static_cast<std::size_t>(rows.size()))
        );
    if (restoredCatalog == nullptr) {
        m_catalogStatusText = "Catalog: Saved cache unreadable, using bundled";
        emit catalogStatusTextChanged();
        return;
    }

    const QString sourceLabel = settings.value(
        settingsKey("catalogSourceLabel"),
        QString("Saved")
    ).toString();
    QString normalizedSourceLabel = stripSavedSuffixes(sourceLabel);
    if (normalizedSourceLabel.isEmpty()) {
        normalizedSourceLabel = "Saved";
    }
    applyCatalog(std::move(restoredCatalog), QString("%1 (saved)").arg(normalizedSourceLabel), false);

    const QByteArray constellationLineRows = settings.value(
        settingsKey("catalogConstellationLineRefs")
    ).toByteArray();
    if (!constellationLineRows.isEmpty()) {
        auto parsedLineRefs = parseConstellationLineRows(
            std::string_view(
                constellationLineRows.constData(),
                static_cast<std::size_t>(constellationLineRows.size())
            )
        );
        if (!parsedLineRefs.empty()) {
            setConstellationLineRefs(std::move(parsedLineRefs));
            emit skyContextChanged();
        }
    }
}
