#pragma once

#include "BaseCelestialBody.hpp"
#include "CelestialBodyState.hpp"
#include "DistantCelestialBody.hpp"
#include "EphemerisSnapshot.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "SkyOverlayLayerVisibility.hpp"
#include "catalog/CatalogCompositionPolicy.hpp"
#include "catalog/CatalogSourceType.hpp"
#include "engine/EphemerisCorrectionFlags.hpp"
#include "engine/EphemerisEngineKind.hpp"

#include <QByteArray>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>

#include <cstddef>
#include <cstdint>
#include <optional>

class SkySettingsStore final {
public:
    struct EphemerisUserSettingsSnapshot final {
        skygate::ephemeris::EphemerisEngineKind::Type engineKind =
            skygate::ephemeris::EphemerisEngineKind::Type::Simple;
        skygate::ephemeris::EphemerisCorrectionFlags correctionFlags =
            skygate::ephemeris::EphemerisCorrectionFlags::apparentTopocentric();
        QString correctionPresetId = QStringLiteral("apparent-topocentric");
        bool fallbackToSimpleEngine = true;
        bool refractionEnabled = true;
        double atmosphericPressureHpa = 1013.25;
        double atmosphericTemperatureC = 10.0;
        double relativeHumidity = 0.0;
        double observingWavelengthMicrometers = 0.55;
        QString preferredDataProfileId = QStringLiteral("de440s-short-range");
        bool onlineUpdatesEnabled = true;
        QString updatePresetId = QStringLiteral("bundled");
        QString updateManifestUrl;
    };

    struct StateSnapshot final {
        bool live = true;
        bool timelineToolbarCollapsed = false;
        bool searchToolbarCollapsed = false;
        double speedMultiplier = 1.0;
        int stepSeconds = 60;
        double magnitudeCutoff = 6.0;
        double viewCenterAltitudeDeg = 0.0;
        double viewCenterAzimuthDeg = 0.0;
        double viewFieldOfViewDeg = 100.0;
        qint64 utcEpochMicros = 0;
        double latitudeDeg = 0.0;
        double longitudeDeg = 0.0;
        double elevationMeters = 0.0;
        QString locationSourceText;
        QString selectedCityId;
        QString displayTimeZoneId;
        QString projectionTypeText;
        QString themeId;
        SkyOverlayLayerVisibility overlayLayers;
        int catalogPresetIndex = 0;
        QString catalogUrlText;
        int deepSkyCatalogPresetIndex = 0;
        QString deepSkyCatalogUrlText;
        bool logToTerminal = true;
        bool logToFile = false;
        QString logFilePath;
        EphemerisUserSettingsSnapshot ephemeris;
        bool ephemerisSettingsPresent = false;
    };

    // Legacy two-slot catalog cache. Retained so existing installations can
    // be migrated to CatalogCollectionCacheSnapshot by
    // SkyCatalogCacheController::migrateLegacy; no new writes use it.
    struct CatalogCacheSnapshot final {
        QString sourceLabel;
        QByteArray catalogPayload;
        QByteArray catalogBinaryPayload;
        QString deepSkySourceLabel;
        QByteArray deepSkyCatalogPayload;
        QByteArray deepSkyBinaryPayload;
        QByteArray constellationLineRows;
        QByteArray constellationAnchorGroupRows;
        int constellationLineSchemaVersion = 0;
        int catalogBinarySchemaVersion = 0;
        std::size_t constellationCount = 0;
    };

    // One persisted catalog source. instanceId is the durable identity; the
    // remaining fields reconstruct the configured source without re-downloading
    // it. payload/binaryPayload are written to sidecar cache files rather than
    // stored inline, so the QSettings file stays small.
    struct CatalogSourceCacheRecord final {
        QString instanceId;
        QString descriptorId;
        QString title;
        QString version;
        QString url;
        QStringList urls;
        QStringList relatedDatasetUrls;
        QString archiveSelector;
        // Parse contract and descriptor metadata required to reproduce the
        // source. Records written before these fields existed load with the
        // defaults: no schema hint, no attribution.
        skygate::ephemeris::CatalogSourceType schemaHint = skygate::ephemeris::CatalogSourceType::Unknown;
        QString attribution;
        skygate::ephemeris::CatalogCompositionPolicy policy = skygate::ephemeris::CatalogCompositionPolicy::Merge;
        bool enabled = true;
        // The source's catalog comes from the bundled factory rather than a
        // stored payload, so the record is configuration only.
        bool bundled = false;
        int order = 0;
        QByteArray payload;
        QByteArray binaryPayload;
        QByteArray constellationLineRows;
        QByteArray constellationAnchorGroupRows;
        int constellationLineSchemaVersion = 0;
        std::size_t constellationCount = 0;
    };

    // Saved even when sources are empty: a stored snapshot is a committed
    // configuration, so an intentionally empty collection stays distinct from
    // "no collection was ever committed" (the Absent load state).
    //
    // A save stages a new generation and publishes it only after every payload
    // file and record is durable, so a failed save leaves the previously
    // committed snapshot loadable instead of mixing old records with new
    // payloads.
    struct CatalogCollectionCacheSnapshot final {
        int schemaVersion = 0;
        int binarySchemaVersion = 0;
        QVector<CatalogSourceCacheRecord> sources;
    };

    // Explicit outcome of reading the committed collection. A snapshot alone
    // cannot distinguish first use from committed configuration that was lost
    // or corrupted, nor a deliberately evicted payload from unreadable
    // committed data.
    struct CatalogCollectionCacheLoadResult final {
        enum class State : std::uint8_t {
            // No collection was ever committed, so a legacy cache may still
            // be migrated by the caller.
            Absent,
            // A committed collection was applied; an empty one is valid.
            Loaded,
            // A committed collection exists but cannot be read.
            Unusable,
        };

        enum class Failure : std::uint8_t {
            None,
            // A durable commit record proves a collection was committed, but
            // its manifest is gone.
            CommittedManifestMissing,
            // The manifest file exists but is incomplete or truncated.
            ManifestUnreadable,
            // The manifest names a generation whose stored records are gone.
            CommittedGenerationMissing,
            // Only the pre-generation version marker survives; its records
            // were replaced by a generation that was never committed.
            LegacyRecordsReplaced,
        };

        State state = State::Absent;
        Failure failure = Failure::None;
        CatalogCollectionCacheSnapshot snapshot;
        // Human-readable context for an unusable result or for uncommitted
        // generation data that was ignored; empty for a clean result.
        QString diagnostic;
        // Records that reference a payload sidecar which is missing or
        // unreadable. A deliberately evicted payload keeps no reference and is
        // not listed here.
        QStringList unreadablePayloadInstanceIds;

        [[nodiscard]] bool isLoaded() const
        {
            return state == State::Loaded;
        }
    };

    struct EphemerisDataCacheSnapshot final {
        QString installedKernelAssetId;
        QString installedKernelProfileId;
        QString installedKernelPath;
        QString installedKernelVersion;
        QString installedEarthOrientationPath;
        QString installedEarthOrientationVersion;
        QString installedLeapSecondTablePath;
        QString installedLeapSecondTableVersion;
        QString installedDeltaTDataPath;
        QString installedDeltaTDataVersion;
        QString dataRevisionToken = QStringLiteral("bundled");
        QString lastUpdateResult = QStringLiteral("Bundled fallback");
    };

public:
    [[nodiscard]] bool saveState(const StateSnapshot& snapshot) const;
    [[nodiscard]] std::optional<StateSnapshot> loadState() const;
    [[nodiscard]] bool saveMainWindowSize(const QSize& size) const;
    [[nodiscard]] QSize loadMainWindowSize() const;
    [[nodiscard]] bool clearCatalogCache() const;
    [[nodiscard]] bool clearDeepSkyCatalogCache() const;
    [[nodiscard]] bool saveCatalogCache(const CatalogCacheSnapshot& snapshot) const;
    [[nodiscard]] std::optional<CatalogCacheSnapshot> loadCatalogCache() const;
    [[nodiscard]] bool saveCatalogCollectionCache(const CatalogCollectionCacheSnapshot& snapshot) const;
    [[nodiscard]] CatalogCollectionCacheLoadResult loadCatalogCollectionCache() const;
    [[nodiscard]] bool clearCatalogCollectionCache() const;
    // Evicts the source's disposable payload files and the payload references
    // in its record while every configuration field stays durable: identity,
    // order, enabled state, descriptor, and parse contract. A restart restores
    // the source as configured with an unavailable payload; only a later
    // accepted load writes payload references again. Removing the source stays
    // the separate configuration-deletion operation.
    [[nodiscard]] bool clearCatalogSourceCache(const QString& instanceId) const;
    [[nodiscard]] bool saveEphemerisDataCache(const EphemerisDataCacheSnapshot& snapshot) const;
    [[nodiscard]] EphemerisDataCacheSnapshot loadEphemerisDataCache() const;
    [[nodiscard]] bool clearEphemerisDataCache() const;
};
