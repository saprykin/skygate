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
constexpr std::size_t kExpectedHygConstellationCount = 88;

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

bool isLikelyHygCatalogSource(const QString& sourceLabel, const QString& catalogUrlText)
{
    const QString normalizedSourceLabel = sourceLabel.trimmed().toLower();
    if (normalizedSourceLabel.contains("hyg")) {
        return true;
    }

    const QString normalizedCatalogUrlText = catalogUrlText.trimmed().toLower();
    return normalizedCatalogUrlText.contains("hyg")
        || normalizedCatalogUrlText.contains("astronexus.com/downloads/catalogs/hygdata");
}
} // namespace

bool SkyContextController::saveSettings() const
{
    QSettings settings;
    settings.setValue(SkyContextSettings::key("version"), SkyContextControllerConstants::kSettingsVersion);
    settings.setValue(SkyContextSettings::key("live"), m_live);
    settings.setValue(SkyContextSettings::key("utcDateLocked"), m_utcDateLocked);
    settings.setValue(SkyContextSettings::key("utcTimeLocked"), m_utcTimeLocked);
    settings.setValue(SkyContextSettings::key("speedMultiplier"), m_speedMultiplier);
    settings.setValue(SkyContextSettings::key("stepSeconds"), m_stepSeconds);
    settings.setValue(SkyContextSettings::key("magnitudeCutoff"), m_magnitudeCutoff);
    settings.setValue(SkyContextSettings::key("viewCenterAltitudeDeg"), m_viewCenterAltitudeDeg);
    settings.setValue(SkyContextSettings::key("viewCenterAzimuthDeg"), m_viewCenterAzimuthDeg);
    settings.setValue(SkyContextSettings::key("viewFieldOfViewDeg"), m_viewFieldOfViewDeg);
    settings.setValue(SkyContextSettings::key("utcEpochSeconds"), SkyContextTimeCodec::toQDateTimeUtc(m_skyContext.utcTime).toSecsSinceEpoch());
    settings.setValue(SkyContextSettings::key("latitudeDeg"), m_skyContext.observer.latitudeDeg);
    settings.setValue(SkyContextSettings::key("longitudeDeg"), m_skyContext.observer.longitudeDeg);
    settings.setValue(SkyContextSettings::key("elevationMeters"), m_skyContext.observer.elevationMeters);
    settings.setValue(SkyContextSettings::key("projectionType"), projectionTypeText());
    settings.setValue(SkyContextSettings::key("catalogSourceLabel"), m_catalogSourceLabel);
    settings.setValue(SkyContextSettings::key("catalogPresetIndex"), m_catalogPresetIndex);
    settings.setValue(SkyContextSettings::key("catalogUrlText"), m_catalogUrlText);
    settings.setValue(
        SkyContextSettings::key("catalogConstellationCount"),
        static_cast<qulonglong>(m_catalogConstellationCount)
    );
    settings.sync();
    return settings.status() == QSettings::NoError;
}

bool SkyContextController::loadSettings()
{
    QSettings settings;
    const bool hasSavedContext = settings.contains(SkyContextSettings::key("version"));
    if (hasSavedContext) {
        const bool live = settings.value(SkyContextSettings::key("live"), m_live).toBool();
        const bool utcDateLocked = settings.value(
            SkyContextSettings::key("utcDateLocked"),
            m_utcDateLocked
        ).toBool();
        const bool utcTimeLocked = settings.value(
            SkyContextSettings::key("utcTimeLocked"),
            m_utcTimeLocked
        ).toBool();
        const double speedMultiplier = settings.value(
            SkyContextSettings::key("speedMultiplier"),
            m_speedMultiplier
        ).toDouble();
        const int stepSeconds = settings.value(
            SkyContextSettings::key("stepSeconds"),
            m_stepSeconds
        ).toInt();
        const double magnitudeCutoff = settings.value(
            SkyContextSettings::key("magnitudeCutoff"),
            m_magnitudeCutoff
        ).toDouble();
        const double viewCenterAltitudeDeg = settings.value(
            SkyContextSettings::key("viewCenterAltitudeDeg"),
            m_viewCenterAltitudeDeg
        ).toDouble();
        const double viewCenterAzimuthDeg = settings.value(
            SkyContextSettings::key("viewCenterAzimuthDeg"),
            m_viewCenterAzimuthDeg
        ).toDouble();
        const double viewFieldOfViewDeg = settings.value(
            SkyContextSettings::key("viewFieldOfViewDeg"),
            m_viewFieldOfViewDeg
        ).toDouble();
        const qint64 utcEpochSeconds = settings.value(
            SkyContextSettings::key("utcEpochSeconds"),
            SkyContextTimeCodec::toQDateTimeUtc(m_skyContext.utcTime).toSecsSinceEpoch()
        ).toLongLong();
        const double latitudeDeg = settings.value(
            SkyContextSettings::key("latitudeDeg"),
            m_skyContext.observer.latitudeDeg
        ).toDouble();
        const double longitudeDeg = settings.value(
            SkyContextSettings::key("longitudeDeg"),
            m_skyContext.observer.longitudeDeg
        ).toDouble();
        const double elevationMeters = settings.value(
            SkyContextSettings::key("elevationMeters"),
            m_skyContext.observer.elevationMeters
        ).toDouble();
        const QString projectionType = settings.value(
            SkyContextSettings::key("projectionType"),
            projectionTypeText()
        ).toString();
        const int catalogPresetIndex = settings.value(
            SkyContextSettings::key("catalogPresetIndex"),
            m_catalogPresetIndex
        ).toInt();
        const QString catalogUrlText = settings.value(
            SkyContextSettings::key("catalogUrlText"),
            m_catalogUrlText
        ).toString();

        setLive(live);
        setUtcDateLocked(utcDateLocked);
        setUtcTimeLocked(utcTimeLocked);
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

bool SkyContextController::clearCatalogCache()
{
    if (m_downloadingCatalog || m_catalogProcessing) {
        m_catalogStatusText = "Catalog: Cannot clear cache while download is in progress";
        emit catalogStatusTextChanged();
        return false;
    }

    QSettings settings;
    const QString configuredPath = settings.value(
        SkyContextSettings::key("catalogCachePath"),
        SkyContextSettings::defaultCatalogCachePath()
    ).toString();
    const QString defaultPath = SkyContextSettings::defaultCatalogCachePath();

    QStringList cachePaths;
    const auto appendCachePath = [&cachePaths](const QString& path) {
        if (!path.isEmpty() && !cachePaths.contains(path)) {
            cachePaths.push_back(path);
        }
    };

    appendCachePath(configuredPath);
    appendCachePath(defaultPath);

    if (!defaultPath.isEmpty()) {
        const QFileInfo defaultInfo(defaultPath);
        const QDir defaultDir(defaultInfo.absolutePath());
        const QStringList matchingEntries = defaultDir.entryList(
            QStringList {"catalog-cache*.txt"},
            QDir::Files | QDir::Readable
        );
        for (const QString& entryName : matchingEntries) {
            appendCachePath(defaultDir.filePath(entryName));
        }
    }

    bool removedAllCacheFiles = true;
    for (const QString& cachePath : cachePaths) {
        QFile cacheFile(cachePath);
        if (cacheFile.exists() && !cacheFile.remove()) {
            removedAllCacheFiles = false;
        }
    }

    settings.remove(SkyContextSettings::key("catalogCachePath"));
    settings.remove(SkyContextSettings::key("catalogSourceLabel"));
    settings.remove(SkyContextSettings::key("catalogConstellationLineRefs"));
    settings.remove(SkyContextSettings::key("catalogConstellationLabelRefs"));
    settings.remove(SkyContextSettings::key("catalogConstellationLineSchemaVersion"));
    settings.remove(SkyContextSettings::key("catalogConstellationCount"));
    settings.sync();
    const bool settingsCleared = settings.status() == QSettings::NoError;
    const bool cacheCleared = removedAllCacheFiles && settingsCleared;

    m_catalogStatusText = cacheCleared
        ? "Catalog: Cache cleared"
        : "Catalog: Cache clear failed";
    emit catalogStatusTextChanged();

    return cacheCleared;
}

void SkyContextController::persistCatalogCache(
    const std::vector<skygate::ephemeris::CelestialBody>& bodies,
    const QString& sourceLabel
) const
{
    if (bodies.empty()) {
        return;
    }

    QByteArray rows = SkyContextCatalogCodec::serializeCatalogRows(bodies);
    if (rows.isEmpty()) {
        return;
    }

    QSettings settings;
    const QString configuredPath = settings.value(
        SkyContextSettings::key("catalogCachePath"),
        SkyContextSettings::defaultCatalogCachePath()
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

    settings.setValue(SkyContextSettings::key("version"), SkyContextControllerConstants::kSettingsVersion);
    settings.setValue(SkyContextSettings::key("catalogCachePath"), configuredPath);
    settings.setValue(SkyContextSettings::key("catalogSourceLabel"), sourceLabel);
    settings.setValue(
        SkyContextSettings::key("catalogConstellationLineRefs"),
        SkyContextCatalogCodec::serializeConstellationLineRows(m_constellationLineRefs)
    );
    settings.setValue(
        SkyContextSettings::key("catalogConstellationLabelRefs"),
        SkyContextCatalogCodec::serializeConstellationLabelRows(m_constellationLabelRefs)
    );
    settings.setValue(
        SkyContextSettings::key("catalogConstellationLineSchemaVersion"),
        SkyContextControllerConstants::kConstellationLineCacheSchemaVersion
    );
    settings.setValue(
        SkyContextSettings::key("catalogConstellationCount"),
        static_cast<qulonglong>(m_catalogConstellationCount)
    );
    settings.sync();
}

void SkyContextController::restoreCatalogCache()
{
    QSettings settings;
    const QString configuredPath = settings.value(
        SkyContextSettings::key("catalogCachePath"),
        SkyContextSettings::defaultCatalogCachePath()
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
        SkyContextSettings::key("catalogSourceLabel"),
        QString("Saved")
    ).toString();
    QString normalizedSourceLabel = stripSavedSuffixes(sourceLabel);
    if (normalizedSourceLabel.isEmpty()) {
        normalizedSourceLabel = "Saved";
    }
    applyCatalog(std::move(restoredCatalog), QString("%1 (saved)").arg(normalizedSourceLabel), false);

    const bool hasPersistedConstellationCount = settings.contains(
        SkyContextSettings::key("catalogConstellationCount")
    );
    const std::size_t persistedConstellationCount = static_cast<std::size_t>(
        settings.value(
            SkyContextSettings::key("catalogConstellationCount"),
            static_cast<qulonglong>(m_catalogConstellationCount)
        ).toULongLong()
    );

    const QByteArray constellationLineRows = settings.value(
        SkyContextSettings::key("catalogConstellationLineRefs")
    ).toByteArray();
    const QByteArray constellationLabelRows = settings.value(
        SkyContextSettings::key("catalogConstellationLabelRefs")
    ).toByteArray();
    const int constellationLineSchemaVersion = settings.value(
        SkyContextSettings::key("catalogConstellationLineSchemaVersion"),
        0
    ).toInt();
    if (
        constellationLineSchemaVersion >= SkyContextControllerConstants::kConstellationLineCacheSchemaVersion
        && !constellationLineRows.isEmpty()
    ) {
        auto parsedLineRefs = SkyContextCatalogCodec::parseConstellationLineRows(
            std::string_view(
                constellationLineRows.constData(),
                static_cast<std::size_t>(constellationLineRows.size())
            )
        );
        if (!parsedLineRefs.empty()) {
            const std::size_t parsedLineRefCount = parsedLineRefs.size();
            setConstellationLineRefs(std::move(parsedLineRefs));
            std::size_t restoredConstellationCount = persistedConstellationCount;
            const bool shouldUseLabelFallback =
                !hasPersistedConstellationCount || restoredConstellationCount == 0;
            if (!constellationLabelRows.isEmpty()) {
                auto parsedLabelRefs = SkyContextCatalogCodec::parseConstellationLabelRows(
                    std::string_view(
                        constellationLabelRows.constData(),
                        static_cast<std::size_t>(constellationLabelRows.size())
                    )
                );
                if (shouldUseLabelFallback && !parsedLabelRefs.empty()) {
                    restoredConstellationCount = parsedLabelRefs.size();
                }
                setConstellationLabelRefs(std::move(parsedLabelRefs));
            } else {
                setConstellationLabelRefs({});
            }
            const bool hasFullLineReferenceSet = parsedLineRefCount >= 200;
            if (
                hasFullLineReferenceSet
                && restoredConstellationCount < kExpectedHygConstellationCount
                && isLikelyHygCatalogSource(m_catalogSourceLabel, m_catalogUrlText)
            ) {
                restoredConstellationCount = kExpectedHygConstellationCount;
            }
            if (restoredConstellationCount != m_catalogConstellationCount) {
                m_catalogConstellationCount = restoredConstellationCount;
                m_catalogStatusText = QString("Catalog: %1 (%2 objects, %3 constellations)").arg(
                    m_catalogSourceLabel,
                    QString::number(static_cast<qulonglong>(m_catalogBodyCount)),
                    QString::number(static_cast<qulonglong>(m_catalogConstellationCount))
                );
                emit catalogStatusTextChanged();
                emit catalogDatasetInfoTextChanged();
            }
            emit skyContextChanged();
        }
    } else if (constellationLineSchemaVersion > 0) {
        resetConstellationLineRefs();
        emit skyContextChanged();
    }
}
