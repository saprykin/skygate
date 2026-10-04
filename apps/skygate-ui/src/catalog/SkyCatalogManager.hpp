#pragma once

#include "SkyCatalogSourceInstance.hpp"
#include "SkyCatalogSourceRecord.hpp"
#include "SkySettingsStore.hpp"

#include "catalog/CatalogCompositionPolicy.hpp"
#include "catalog/IStarCatalog.hpp"
#include "catalog/constellation/ConstellationData.hpp"

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

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
struct SkyCatalogSourceImportResult;
struct SkyDeepSkyCatalogImportResult;
struct SkyConstellationLineImportResult;
struct SkyCatalogSourceInstance;
}  // namespace skygate::ui::internal

class SkyCatalogManager final : public QObject {
    Q_OBJECT

public:
    using ConstellationLineRef = skygate::ephemeris::ConstellationLineRef;
    using ConstellationAnchorGroup = skygate::ephemeris::ConstellationAnchorGroup;

    struct SourceViewEntry final {
        QString instanceId;
        QString title;
        QString version;
        skygate::ephemeris::CatalogCompositionPolicy policy = skygate::ephemeris::CatalogCompositionPolicy::Merge;
        bool enabled = true;
        bool bundled = false;
        bool busy = false;
        bool hasError = false;
        QString statusText;
        std::size_t objectCount = 0;
    };

    explicit SkyCatalogManager(
        SkySettingsStore* settingsStore,
        std::unique_ptr<skygate::ephemeris::IStarCatalog> starCatalog = nullptr,
        QObject* parent = nullptr,
        QNetworkAccessManager* networkAccessManager = nullptr
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
    [[nodiscard]] std::size_t sourceCount() const noexcept;
    [[nodiscard]] QStringList sourceInstanceIds() const;
    [[nodiscard]] bool isSourceEnabled(const QString& instanceId) const;
    [[nodiscard]] QStringList sourceLabels() const;
    [[nodiscard]] QVector<SourceViewEntry> sourceViewEntries() const;
    [[nodiscard]] std::span<const std::uint8_t> sourceIds() const noexcept;
    [[nodiscard]] std::span<const ConstellationLineRef> constellationLineRefs() const noexcept;
    [[nodiscard]] std::span<const ConstellationAnchorGroup> constellationAnchorGroups() const noexcept;
    [[nodiscard]] std::span<const ConstellationLineRef> resolvedConstellationLineRefs() const;
    [[nodiscard]] std::span<const ConstellationAnchorGroup> resolvedConstellationAnchorGroups() const;

    void setCatalogPresetIndex(int catalogPresetIndex);
    void setCatalogUrlText(const QString& catalogUrlText);
    void setDeepSkyCatalogPresetIndex(int deepSkyCatalogPresetIndex);
    void setDeepSkyCatalogUrlText(const QString& deepSkyCatalogUrlText);
    void loadCatalogPreset(const QString& presetId);
    void downloadCatalogFromUrl(const QString& urlText);
    void loadDeepSkyCatalogPreset(const QString& presetId);
    void downloadDeepSkyCatalogFromUrl(const QString& urlText);
    void loadSource(
        const skygate::ui::internal::SkyCatalogSourceInstance& source,
        skygate::ephemeris::CatalogCompositionPolicy policy
    );
    void addSourcePreset(const QString& presetId);
    void addSourceUrl(const QString& urlText, const QString& category);
    void enableSource(const QString& instanceId);
    void disableSource(const QString& instanceId);
    void removeSource(const QString& instanceId);
    void moveSource(const QString& instanceId, int targetIndex);
    void retrySource(const QString& instanceId);
    void cancelCatalogDownload();
    bool clearCatalogCache();
    bool clearDeepSkyCatalogCache();
    bool clearSourceCache(const QString& instanceId);
    bool restoreCatalogCache();

signals:
    void statusTextChanged();
    void datasetInfoTextChanged();
    void deepSkyCatalogInfoTextChanged();
    void downloadingCatalogChanged();
    void catalogProcessingChanged();
    void catalogChanged();
    void sourcesChanged();

private:
    struct SourceOperation final {
        skygate::ui::internal::SkyCatalogSourceInstance instance;
        skygate::ephemeris::CatalogCompositionPolicy policy = skygate::ephemeris::CatalogCompositionPolicy::Merge;
        QByteArray payload;
        std::uint64_t revision = 0;
        bool constellationPending = false;
        bool busy = false;
        bool hasError = false;
        QString statusText;
    };

    void loadSourceInstance(
        const skygate::ui::internal::SkyCatalogSourceInstance& source,
        skygate::ephemeris::CatalogCompositionPolicy policy
    );
    void applyBundledSource(SourceOperation& operation, skygate::ephemeris::CatalogCompositionPolicy policy);
    void applySourceResult(
        skygate::ui::internal::SkyCatalogSourceImportResult result, skygate::ephemeris::CatalogCompositionPolicy policy
    );
    void handleSourceImportFinished(
        const QString& instanceId,
        std::uint64_t revision,
        skygate::ui::internal::SkyCatalogSourceImportResult result,
        skygate::ephemeris::CatalogCompositionPolicy policy,
        const QStringList& relatedDatasetUrls
    );
    [[nodiscard]] SourceOperation* upsertOperation(
        const skygate::ui::internal::SkyCatalogSourceInstance& source,
        skygate::ephemeris::CatalogCompositionPolicy policy
    );
    [[nodiscard]] SourceOperation* findOperation(const QString& instanceId);
    [[nodiscard]] const SourceOperation* findOperation(const QString& instanceId) const;
    void removeOperation(const QString& instanceId);
    [[nodiscard]] bool isOperationCurrent(const QString& instanceId, std::uint64_t revision) const;
    void invalidatePendingSourceWork();
    void setSourceEnabled(const QString& instanceId, bool enabled);
    void setStatusText(const QString& statusText);
    void setDownloadingCatalog(bool downloadingCatalog);
    void setCatalogProcessing(bool catalogProcessing);
    void handleCatalogImportStatus(const QString& instanceId, const QString& statusText);
    void downloadConstellationLinesAfterCatalog(
        const QString& instanceId,
        std::uint64_t revision,
        const QStringList& constellationLineUrlTexts,
        const QString& catalogSummaryText
    );
    void handleConstellationLineImportStatus(const QString& catalogSummaryText, const QString& statusText);
    void handleConstellationLineImportFinished(
        const QString& catalogSummaryText, skygate::ui::internal::SkyConstellationLineImportResult lineResult
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
    bool m_constellationDownloadPending = false;
    QString m_activeDownloadInstanceId;
    QVector<SourceOperation> m_sourceOperations;
};
