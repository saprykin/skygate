#pragma once

#include "SkyCatalogConstellationStore.hpp"
#include "SkyCatalogSourceRecord.hpp"

#include "catalog/IStarCatalog.hpp"

#include <QHash>
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
    // Explicit participation of the bundled deep-sky catalog in this
    // composition.
    //
    // Disabled: bundled deep-sky bodies do not participate.
    // Fallback: bundled deep-sky bodies fill only the deep-sky identities no
    // enabled configured source supplies. The fallback never replaces a
    // configured source's body or its position in the visible collection, and
    // its contribution is reported under its own provenance.
    enum class BundledDeepSkyParticipation : std::uint8_t {
        Disabled,
        Fallback
    };

    BundledDeepSkyParticipation bundledDeepSkyParticipation = BundledDeepSkyParticipation::Disabled;
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
    [[nodiscard]] bool hasSource(const QString& instanceId) const;
    [[nodiscard]] QHash<QString, QString> sourceTitles() const;
    [[nodiscard]] QString sourceTitle(const QString& instanceId) const;
    [[nodiscard]] std::span<const SkyCatalogSourceRecord> sources() const noexcept;
    [[nodiscard]] std::span<const QString> sourceIds() const noexcept;
    [[nodiscard]] const std::vector<QStringList>& contributorSourceIds() const noexcept;
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
    [[nodiscard]] SkyCatalogRuntimeResult
    moveSource(const QString& instanceId, std::size_t targetIndex, const SkyCatalogRuntimeBuildOptions& options);
    [[nodiscard]] SkyCatalogRuntimeResult
    replaceSources(std::vector<SkyCatalogSourceRecord> sources, const SkyCatalogRuntimeBuildOptions& options);
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

private:
    [[nodiscard]] SkyCatalogRuntimeResult failedCatalogResult(const QString& statusText);
    [[nodiscard]] QString buildStatusText() const;
    [[nodiscard]] QString deepSkySourceLabel() const;
    [[nodiscard]] const SkyCatalogSourceRecord* findSource(const QString& instanceId) const;
    [[nodiscard]] const SkyCatalogSourceRecord*
    firstSourceWithPolicy(skygate::ephemeris::CatalogCompositionPolicy policy, bool requireEnabled) const;
    void rebuildSourceProvenance(
        const std::vector<std::string>& composedSourceIds,
        const std::vector<std::vector<std::string>>& contributorSourceIds
    );
    void refreshResolvedConstellationRefs() const;

private:
    std::unique_ptr<skygate::ephemeris::IStarCatalog> m_starCatalog;
    std::vector<SkyCatalogSourceRecord> m_sources;
    std::uint64_t m_catalogRevision = 0;
    std::size_t m_bodyCount = 0;
    std::size_t m_deepSkyObjectCount = 0;
    std::size_t m_deepSkyCatalogFoundObjectCount = 0;
    QHash<QString, QString> m_sourceTitles;
    std::vector<QString> m_sourceIds;
    std::vector<QStringList> m_contributorSourceIds;
    SkyCatalogConstellationStore m_constellationRefs;
    mutable std::vector<ConstellationLineRef> m_resolvedLineRefs;
    mutable std::vector<ConstellationAnchorGroup> m_resolvedAnchorGroups;
    mutable std::uint64_t m_resolvedRevision = 0U;
};

}  // namespace skygate::ui::internal
