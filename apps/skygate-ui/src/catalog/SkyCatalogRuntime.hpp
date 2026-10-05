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
    // False when the requested transition was rejected and nothing was
    // published: statusText reports the operation error. The last accepted
    // configuration, snapshot, counts, provenance, related view, and revision
    // are unchanged.
    bool succeeded = true;
};

// Owns the configured source collection, the active composed snapshot, and the
// per-source related constellation datasets that feed the active related view.
//
// Related constellation data belongs to the source instance whose related
// download produced it. The active view composes the owned datasets of the
// enabled sources in visible collection order, the same order used for object
// composition:
//
// - Line segments from every enabled contributor are kept in visible order; an
//   exact duplicate segment from a later contributor is kept once.
// - Anchor groups are identified by their constellation name. When two enabled
//   contributors define the same name, the later contributor's definition
//   replaces the earlier one, matching the later-wins precedence used for
//   overlapping object identities.
// - The declared constellation count of the active view is taken from the last
//   enabled contributor that declares one, and is never lower than the number
//   of distinct constellation names in the view.
//
// An owned dataset is retained while its source is disabled and is removed
// with its source. References are resolved through the shared
// ConstellationReferenceResolver against the active snapshot; the resolved
// view is invalidated whenever either the snapshot or the active related view
// changes.
//
// Activation is a single transition. A source mutation or rebuild composes the
// candidate configuration and every derived value before anything is
// published, so the configured sources, active snapshot, counts, provenance,
// related view, and revision only change together, and only for an accepted
// composition. A rejected transition - a null catalog, a composition
// rejection, or a missing bundled catalog - reports the operation error through
// the result and keeps the last accepted configuration and snapshot exactly as
// they were; the rejected configuration is not installed. Every accepted
// transition increments catalogRevision() and reports catalogChanged.
class SkyCatalogRuntime final {
public:
    using ConstellationLineRef = skygate::ephemeris::ConstellationLineRef;
    using ConstellationAnchorGroup = skygate::ephemeris::ConstellationAnchorGroup;

public:
    explicit SkyCatalogRuntime(std::unique_ptr<skygate::ephemeris::IStarCatalog> sourceCatalog);

    [[nodiscard]] const skygate::ephemeris::IStarCatalog* starCatalog() const noexcept;
    // Title of the leading configured source that participates. When every
    // configured source is disabled this is the leading implicit bundled
    // contribution, so a disabled source is never presented as the active one.
    [[nodiscard]] QString sourceLabel() const;
    // Composition summary of the currently published state, in the same form a
    // successful transition result carries.
    [[nodiscard]] QString statusText() const;
    // Presentation summary of the collection's actual participation: the
    // enabled configured sources in visible collection order, followed by the
    // bundled contributions whose objects survive in the active snapshot.
    //
    // A configured but disabled source never appears, and an implicit bundled
    // contribution is named only while it supplies objects, so the summary
    // never claims a source that does not participate. The bundled names come
    // from the recorded provenance instead of a hard-coded catalog assumption.
    [[nodiscard]] QString participationSummary() const;
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
    // Replaces the related constellation dataset owned by instanceId. An
    // unknown instance ID changes nothing. The dataset stays inactive while
    // the owning source is disabled.
    [[nodiscard]] SkyCatalogRuntimeResult setSourceConstellationRefs(
        const QString& instanceId,
        std::vector<ConstellationLineRef> lineRefs,
        std::vector<ConstellationAnchorGroup> anchorGroups,
        std::size_t constellationCount
    );
    // Drops the related constellation dataset owned by instanceId, used when
    // the owner's catalog is reloaded or reset.
    [[nodiscard]] SkyCatalogRuntimeResult clearSourceConstellationRefs(const QString& instanceId);

private:
    // Builds the result of a rejected transition: it reports the operation
    // error without publishing or discarding any state.
    [[nodiscard]] SkyCatalogRuntimeResult activationFailureResult(const QString& statusText);
    [[nodiscard]] QString buildStatusText() const;
    [[nodiscard]] SkyCatalogSourceRecord* findSource(const QString& instanceId);
    [[nodiscard]] const SkyCatalogSourceRecord* findSource(const QString& instanceId) const;
    // Composes the active related view from the owned datasets of the enabled
    // sources and returns the composed declared constellation count.
    [[nodiscard]] std::size_t buildActiveConstellationView(
        std::vector<ConstellationLineRef>& lineRefs, std::vector<ConstellationAnchorGroup>& anchorGroups
    ) const;
    // Recomposes the active related view after an owned dataset changed.
    [[nodiscard]] SkyCatalogRuntimeResult refreshActiveConstellationView();
    void rebuildSourceProvenance(
        const std::vector<std::string>& composedSourceIds,
        const std::vector<std::vector<std::string>>& contributorSourceIds
    );
    void rebuildSourceParticipation(const std::vector<std::string>& contributingSourceIds);
    void refreshResolvedConstellationRefs() const;

private:
    std::unique_ptr<skygate::ephemeris::IStarCatalog> m_starCatalog;
    std::vector<SkyCatalogSourceRecord> m_sources;
    std::uint64_t m_catalogRevision = 0;
    std::size_t m_bodyCount = 0;
    std::size_t m_catalogConstellationCount = 0;
    std::size_t m_deepSkyObjectCount = 0;
    std::size_t m_deepSkyCatalogFoundObjectCount = 0;
    QHash<QString, QString> m_sourceTitles;
    std::vector<QString> m_sourceIds;
    // Instance IDs of the sources that supplied at least one object of the
    // active snapshot, in precedence order, exactly as reported by the last
    // accepted composition.
    std::vector<QString> m_contributingSourceIds;
    std::vector<QStringList> m_contributorSourceIds;
    SkyCatalogConstellationStore m_constellationRefs;
    mutable std::vector<ConstellationLineRef> m_resolvedLineRefs;
    mutable std::vector<ConstellationAnchorGroup> m_resolvedAnchorGroups;
    mutable std::uint64_t m_resolvedRevision = 0U;
};

}  // namespace skygate::ui::internal
