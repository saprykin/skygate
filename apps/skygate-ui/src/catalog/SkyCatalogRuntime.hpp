#pragma once

#include "SkyCatalogCacheController.hpp"
#include "SkyCatalogConstellationStore.hpp"
#include "SkyCatalogSourceRecord.hpp"

#include "catalog/IStarCatalog.hpp"

#include <QString>
#include <QStringList>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace skygate::ui::internal {

struct SkyCatalogRuntimeBuildOptions final {
    bool useBundledDeepSkyCatalog = false;
};

struct SkyCatalogRuntimeResult final {
    QString statusText;
    bool statusTextChanged = false;
    bool datasetInfoChanged = false;
    bool deepSkyCatalogInfoChanged = false;
    bool catalogChanged = false;
};

class SkyCatalogRuntime final {
public:
    using ConstellationLineRef = skygate::ephemeris::ConstellationLineRef;
    using ConstellationAnchorGroup = skygate::ephemeris::ConstellationAnchorGroup;

public:
    explicit SkyCatalogRuntime(std::unique_ptr<skygate::ephemeris::IStarCatalog> sourceCatalog);

    [[nodiscard]] const skygate::ephemeris::IStarCatalog* starCatalog() const noexcept;
    [[nodiscard]] QString sourceLabel() const;
    [[nodiscard]] std::size_t bodyCount() const noexcept;
    [[nodiscard]] std::size_t constellationCount() const noexcept;
    [[nodiscard]] std::size_t deepSkyObjectCount() const noexcept;
    [[nodiscard]] std::size_t deepSkyCatalogFoundObjectCount() const noexcept;
    [[nodiscard]] std::uint64_t catalogRevision() const noexcept;
    [[nodiscard]] std::size_t sourceCount() const noexcept;
    [[nodiscard]] QStringList sourceInstanceIds() const;
    [[nodiscard]] bool isSourceEnabled(const QString& instanceId) const;
    [[nodiscard]] QStringList sourceLabels() const;
    [[nodiscard]] std::span<const std::uint8_t> sourceIds() const noexcept;
    [[nodiscard]] std::span<const ConstellationLineRef> constellationLineRefs() const noexcept;
    [[nodiscard]] std::span<const ConstellationAnchorGroup> constellationAnchorGroups() const noexcept;
    [[nodiscard]] std::span<const ConstellationLineRef> resolvedConstellationLineRefs() const;
    [[nodiscard]] std::span<const ConstellationAnchorGroup> resolvedConstellationAnchorGroups() const;

    [[nodiscard]] SkyCatalogRuntimeResult initialize(const SkyCatalogRuntimeBuildOptions& options);
    [[nodiscard]] SkyCatalogRuntimeResult
    applySource(SkyCatalogSourceRecord source, const SkyCatalogRuntimeBuildOptions& options);
    [[nodiscard]] SkyCatalogRuntimeResult
    setSourceEnabled(const QString& instanceId, bool enabled, const SkyCatalogRuntimeBuildOptions& options);
    [[nodiscard]] SkyCatalogRuntimeResult
    removeSource(const QString& instanceId, const SkyCatalogRuntimeBuildOptions& options);
    [[nodiscard]] SkyCatalogRuntimeResult applyCatalog(
        std::unique_ptr<skygate::ephemeris::IStarCatalog> catalog,
        const QString& sourceLabel,
        const SkyCatalogRuntimeBuildOptions& options
    );
    [[nodiscard]] SkyCatalogRuntimeResult applyDeepSkyCatalog(
        std::unique_ptr<skygate::ephemeris::IStarCatalog> catalog,
        const QString& sourceLabel,
        std::size_t foundObjectCount,
        const SkyCatalogRuntimeBuildOptions& options
    );
    [[nodiscard]] SkyCatalogRuntimeResult
    clearDeepSkyCatalog(const QString& sourceLabel, const SkyCatalogRuntimeBuildOptions& options);
    [[nodiscard]] SkyCatalogRuntimeResult rebuildActiveCatalog(const SkyCatalogRuntimeBuildOptions& options);
    [[nodiscard]] SkyCatalogRuntimeResult resetConstellationLineRefs();
    [[nodiscard]] SkyCatalogRuntimeResult setConstellationLineRefs(std::vector<ConstellationLineRef> lineRefs);
    [[nodiscard]] SkyCatalogRuntimeResult
    setConstellationAnchorGroups(std::vector<ConstellationAnchorGroup> anchorGroups);
    [[nodiscard]] SkyCatalogRuntimeResult restoreConstellationRefs(
        std::vector<ConstellationLineRef> lineRefs,
        std::vector<ConstellationAnchorGroup> anchorGroups,
        std::optional<std::size_t> constellationCount
    );
    [[nodiscard]] std::optional<SkyCatalogCachePersistRequest>
    cachePersistRequest(const QByteArray& catalogPayload, const QByteArray& deepSkyCatalogPayload) const;

private:
    [[nodiscard]] SkyCatalogRuntimeResult failedCatalogResult(const QString& statusText);
    [[nodiscard]] SkyCatalogRuntimeResult failedDeepSkyCatalogResult(const QString& statusText);
    [[nodiscard]] QString buildStatusText() const;
    [[nodiscard]] QString deepSkySourceLabel() const;
    [[nodiscard]] const SkyCatalogSourceRecord* findSource(const QString& instanceId) const;
    [[nodiscard]] const SkyCatalogSourceRecord*
    firstSourceWithPolicy(skygate::ephemeris::CatalogCompositionPolicy policy, bool requireEnabled) const;
    void rebuildSourceProvenance(const std::vector<std::string>& composedSourceIds);
    void refreshResolvedConstellationRefs() const;

private:
    std::unique_ptr<skygate::ephemeris::IStarCatalog> m_starCatalog;
    std::vector<SkyCatalogSourceRecord> m_sources;
    std::uint64_t m_catalogRevision = 0;
    std::size_t m_bodyCount = 0;
    std::size_t m_deepSkyObjectCount = 0;
    std::size_t m_deepSkyCatalogFoundObjectCount = 0;
    QStringList m_sourceLabels;
    std::vector<std::uint8_t> m_sourceIds;
    SkyCatalogConstellationStore m_constellationRefs;
    mutable std::vector<ConstellationLineRef> m_resolvedLineRefs;
    mutable std::vector<ConstellationAnchorGroup> m_resolvedAnchorGroups;
    mutable std::uint64_t m_resolvedRevision = 0U;
};

}  // namespace skygate::ui::internal
