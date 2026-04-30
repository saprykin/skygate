#pragma once

#include "SkySettingsStore.hpp"

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>

#include "skygate/ephemeris/ConstellationData.hpp"
#include "skygate/ephemeris/IEphemerisEngine.hpp"
#include "skygate/ephemeris/IStarCatalog.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

class QNetworkAccessManager;
class CatalogCoordinator;

class SkyCatalogManager final : public QObject {
    Q_OBJECT

public:
    using ConstellationLineRef = skygate::ephemeris::ConstellationLineRef;
    using ConstellationLabelRef = skygate::ephemeris::ConstellationLabelRef;

    explicit SkyCatalogManager(
        SkySettingsStore* settingsStore,
        std::unique_ptr<skygate::ephemeris::IStarCatalog> starCatalog = nullptr,
        std::unique_ptr<skygate::ephemeris::IEphemerisEngine> ephemerisEngine = nullptr,
        QObject* parent = nullptr
    );
    ~SkyCatalogManager() override;

    [[nodiscard]] QString statusText() const;
    [[nodiscard]] QString datasetInfoText() const;
    [[nodiscard]] QString deepSkyCatalogInfoText() const;
    [[nodiscard]] bool downloadingCatalog() const noexcept;
    [[nodiscard]] bool catalogProcessing() const noexcept;
    [[nodiscard]] int catalogPresetIndex() const noexcept;
    [[nodiscard]] QString catalogUrlText() const;
    [[nodiscard]] int deepSkyCatalogPresetIndex() const noexcept;
    [[nodiscard]] QString deepSkyCatalogUrlText() const;
    [[nodiscard]] QString sourceLabel() const;
    [[nodiscard]] std::size_t bodyCount() const noexcept;
    [[nodiscard]] std::size_t constellationCount() const noexcept;
    [[nodiscard]] std::uint64_t catalogRevision() const noexcept;
    [[nodiscard]] const skygate::ephemeris::IStarCatalog* starCatalog() const noexcept;
    [[nodiscard]] const skygate::ephemeris::IEphemerisEngine* ephemerisEngine() const noexcept;
    [[nodiscard]] std::span<const ConstellationLineRef> constellationLineRefs() const noexcept;
    [[nodiscard]] std::span<const ConstellationLabelRef> constellationLabelRefs() const noexcept;

    void setCatalogPresetIndex(int catalogPresetIndex);
    void setCatalogUrlText(const QString& catalogUrlText);
    void setDeepSkyCatalogPresetIndex(int deepSkyCatalogPresetIndex);
    void setDeepSkyCatalogUrlText(const QString& deepSkyCatalogUrlText);
    void loadCatalogPreset(const QString& presetId);
    void downloadCatalogFromUrl(const QString& urlText);
    void loadDeepSkyCatalogPreset(const QString& presetId);
    void downloadDeepSkyCatalogFromUrl(const QString& urlText);
    bool clearCatalogCache();
    bool clearDeepSkyCatalogCache();
    bool restoreCatalogCache();

signals:
    void statusTextChanged();
    void datasetInfoTextChanged();
    void deepSkyCatalogInfoTextChanged();
    void downloadingCatalogChanged();
    void catalogProcessingChanged();
    void catalogChanged();

private:
    void applyCatalog(
        std::unique_ptr<skygate::ephemeris::IStarCatalog> catalog,
        const QString& sourceLabel,
        bool persistCatalog = true
    );
    void applyDeepSkyCatalog(
        std::unique_ptr<skygate::ephemeris::IStarCatalog> catalog,
        const QString& sourceLabel,
        bool persistCatalog = true
    );
    void rebuildActiveCatalog(bool persistCatalog);
    void downloadCatalogFromUrls(
        const QStringList& urlTexts,
        const QString& sourceLabel,
        const QStringList& constellationLineUrlTexts = {}
    );
    void downloadDeepSkyCatalogFromUrls(
        const QStringList& urlTexts,
        const QString& sourceLabel
    );
    void resetConstellationLineRefs();
    void setConstellationLineRefs(std::vector<ConstellationLineRef> lineRefs);
    void setConstellationLabelRefs(std::vector<ConstellationLabelRef> labelRefs);
    void persistCatalogCache() const;

private:
    SkySettingsStore* m_settingsStore = nullptr;
    std::unique_ptr<skygate::ephemeris::IStarCatalog> m_starCatalog;
    std::unique_ptr<skygate::ephemeris::IStarCatalog> m_sourceCatalog;
    std::unique_ptr<skygate::ephemeris::IStarCatalog> m_deepSkyCatalog;
    std::unique_ptr<skygate::ephemeris::IEphemerisEngine> m_ephemerisEngine;
    std::unique_ptr<CatalogCoordinator> m_catalogCoordinator;
    QNetworkAccessManager* m_networkAccessManager = nullptr;
    QString m_statusText;
    QString m_sourceLabel = "Bundled";
    QString m_deepSkySourceLabel = "Bundled Messier";
    int m_catalogPresetIndex = 0;
    int m_deepSkyCatalogPresetIndex = 0;
    QString m_catalogUrlText;
    QString m_deepSkyCatalogUrlText;
    std::uint64_t m_catalogRevision = 0;
    bool m_downloadingCatalog = false;
    bool m_catalogProcessing = false;
    std::size_t m_bodyCount = 0;
    std::size_t m_constellationCount = 0;
    std::size_t m_deepSkyObjectCount = 0;
    std::size_t m_deepSkyCatalogFoundObjectCount = 0;
    QByteArray m_cachedCatalogPayload;
    QByteArray m_cachedDeepSkyCatalogPayload;
    std::vector<ConstellationLineRef> m_constellationLineRefs;
    std::vector<ConstellationLabelRef> m_constellationLabelRefs;
};
