#pragma once

#include "SkySettingsStore.hpp"

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>

#include "skygate/ephemeris/IEphemerisEngine.hpp"
#include "skygate/ephemeris/IStarCatalog.hpp"
#include "skygate/ephemeris/ConstellationData.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

class QNetworkAccessManager;

namespace skygate::ui::internal {
class SkyCatalogCacheController;
class SkyCatalogImportWorkflow;
class SkyCatalogRuntime;
struct SkyCatalogRuntimeBuildOptions;
struct SkyCatalogRuntimeResult;
struct SkyCatalogImportResult;
struct SkyDeepSkyCatalogImportResult;
struct SkyConstellationLineImportResult;
}

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
    [[nodiscard]] QStringList sourceLabels() const;
    [[nodiscard]] std::span<const std::uint8_t> sourceIds() const noexcept;
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
        std::size_t foundObjectCount = 0,
        bool persistCatalog = true
    );
    void downloadCatalogFromUrls(
        const QStringList& urlTexts,
        const QString& sourceLabel,
        const QStringList& constellationLineUrlTexts = {}
    );
    void downloadDeepSkyCatalogFromUrls(
        const QStringList& urlTexts,
        const QString& sourceLabel
    );
    void setStatusText(const QString& statusText);
    void setDownloadingCatalog(bool downloadingCatalog);
    void setCatalogProcessing(bool catalogProcessing);
    void handleCatalogImportStatus(const QString& statusText);
    void handleCatalogImportFinished(
        skygate::ui::internal::SkyCatalogImportResult result,
        const QStringList& constellationLineUrlTexts
    );
    void downloadConstellationLinesAfterCatalog(
        const QStringList& constellationLineUrlTexts,
        const QString& catalogSummaryText
    );
    void handleConstellationLineImportStatus(
        const QString& catalogSummaryText,
        const QString& statusText
    );
    void handleConstellationLineImportFinished(
        const QString& catalogSummaryText,
        skygate::ui::internal::SkyConstellationLineImportResult lineResult
    );
    void handleDeepSkyImportFinished(
        skygate::ui::internal::SkyDeepSkyCatalogImportResult result
    );
    [[nodiscard]] skygate::ui::internal::SkyCatalogRuntimeBuildOptions runtimeBuildOptions() const;
    void applyRuntimeResult(const skygate::ui::internal::SkyCatalogRuntimeResult& result);
    void resetConstellationLineRefs();
    void persistCatalogCache() const;

private:
    std::unique_ptr<skygate::ui::internal::SkyCatalogRuntime> m_runtime;
    std::unique_ptr<skygate::ui::internal::SkyCatalogCacheController> m_cacheController;
    std::unique_ptr<skygate::ui::internal::SkyCatalogImportWorkflow> m_importWorkflow;
    QNetworkAccessManager* m_networkAccessManager = nullptr;
    QString m_statusText;
    int m_catalogPresetIndex = 0;
    int m_deepSkyCatalogPresetIndex = 0;
    QString m_catalogUrlText;
    QString m_deepSkyCatalogUrlText;
    bool m_downloadingCatalog = false;
    bool m_catalogProcessing = false;
    QByteArray m_cachedCatalogPayload;
    QByteArray m_cachedDeepSkyCatalogPayload;
};
