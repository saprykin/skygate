#pragma once

#include "SkyCatalogCacheController.hpp"
#include "SkyCatalogConstellationStore.hpp"

#include "skygate/ephemeris/IEphemerisEngine.hpp"
#include "skygate/ephemeris/IStarCatalog.hpp"

#include <QString>
#include <QStringList>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
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
    using ConstellationLabelRef = skygate::ephemeris::ConstellationLabelRef;

public:
    SkyCatalogRuntime(
        std::unique_ptr<skygate::ephemeris::IStarCatalog> sourceCatalog,
        std::unique_ptr<skygate::ephemeris::IEphemerisEngine> ephemerisEngine
    );

    [[nodiscard]] const skygate::ephemeris::IStarCatalog* starCatalog() const noexcept;
    [[nodiscard]] const skygate::ephemeris::IEphemerisEngine* ephemerisEngine() const noexcept;
    [[nodiscard]] QString sourceLabel() const;
    [[nodiscard]] std::size_t bodyCount() const noexcept;
    [[nodiscard]] std::size_t constellationCount() const noexcept;
    [[nodiscard]] std::size_t deepSkyObjectCount() const noexcept;
    [[nodiscard]] std::size_t deepSkyCatalogFoundObjectCount() const noexcept;
    [[nodiscard]] std::uint64_t catalogRevision() const noexcept;
    [[nodiscard]] QStringList sourceLabels() const;
    [[nodiscard]] std::span<const std::uint8_t> sourceIds() const noexcept;
    [[nodiscard]] std::span<const ConstellationLineRef> constellationLineRefs() const noexcept;
    [[nodiscard]] std::span<const ConstellationLabelRef> constellationLabelRefs() const noexcept;

    [[nodiscard]] SkyCatalogRuntimeResult initialize(const SkyCatalogRuntimeBuildOptions& options);
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
    [[nodiscard]] SkyCatalogRuntimeResult clearDeepSkyCatalog(
        const QString& sourceLabel,
        const SkyCatalogRuntimeBuildOptions& options
    );
    [[nodiscard]] SkyCatalogRuntimeResult rebuildActiveCatalog(
        const SkyCatalogRuntimeBuildOptions& options
    );
    [[nodiscard]] SkyCatalogRuntimeResult resetConstellationLineRefs();
    [[nodiscard]] SkyCatalogRuntimeResult setConstellationLineRefs(
        std::vector<ConstellationLineRef> lineRefs
    );
    [[nodiscard]] SkyCatalogRuntimeResult setConstellationLabelRefs(
        std::vector<ConstellationLabelRef> labelRefs
    );
    [[nodiscard]] SkyCatalogRuntimeResult restoreConstellationRefs(
        std::vector<ConstellationLineRef> lineRefs,
        std::vector<ConstellationLabelRef> labelRefs,
        std::optional<std::size_t> constellationCount
    );
    [[nodiscard]] std::optional<SkyCatalogCachePersistRequest> cachePersistRequest(
        const QByteArray& catalogPayload,
        const QByteArray& deepSkyCatalogPayload
    ) const;

private:
    [[nodiscard]] SkyCatalogRuntimeResult failedCatalogResult(const QString& statusText);
    [[nodiscard]] SkyCatalogRuntimeResult failedDeepSkyCatalogResult(const QString& statusText);

private:
    std::unique_ptr<skygate::ephemeris::IStarCatalog> m_starCatalog;
    std::unique_ptr<skygate::ephemeris::IStarCatalog> m_sourceCatalog;
    std::unique_ptr<skygate::ephemeris::IStarCatalog> m_deepSkyCatalog;
    std::unique_ptr<skygate::ephemeris::IEphemerisEngine> m_ephemerisEngine;
    QString m_sourceLabel = QStringLiteral("Bundled");
    QString m_deepSkySourceLabel = QStringLiteral("Bundled Messier");
    std::uint64_t m_catalogRevision = 0;
    std::size_t m_bodyCount = 0;
    std::size_t m_deepSkyObjectCount = 0;
    std::size_t m_deepSkyCatalogFoundObjectCount = 0;
    QStringList m_sourceLabels;
    std::vector<std::uint8_t> m_sourceIds;
    SkyCatalogConstellationStore m_constellationRefs;
};

}  // namespace skygate::ui::internal
