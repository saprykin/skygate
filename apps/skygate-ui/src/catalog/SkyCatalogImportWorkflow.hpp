#pragma once

#include "SkyCatalogSourceInstance.hpp"

#include "catalog/CatalogCompositionPolicy.hpp"
#include "catalog/CatalogLoadResult.hpp"
#include "catalog/IStarCatalog.hpp"
#include "catalog/constellation/ConstellationData.hpp"

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <cstddef>
#include <functional>
#include <memory>
#include <vector>

class QNetworkAccessManager;
class QObject;
class CatalogCoordinator;

namespace skygate::ui::internal {

struct SkyCatalogSourceImportResult final {
    QByteArray payload;
    std::unique_ptr<skygate::ephemeris::IStarCatalog> catalog;
    skygate::ephemeris::CatalogLoadDiagnostics diagnostics;
    std::size_t foundObjectCount = 0;
    QString sourceLabel;
    QString sourceId;
    QString sourceVersion;
    QString sourceUrl;
    QString errorText;
};

struct SkyConstellationLineImportResult final {
    std::vector<skygate::ephemeris::ConstellationLineRef> lineRefs;
    std::vector<skygate::ephemeris::ConstellationAnchorGroup> anchorGroups;
    std::size_t constellationCount = 0;
    QString statusSuffix;

    [[nodiscard]] bool hasCustomLines() const noexcept;
};

// Downloads and parses one configured source instance.
//
// The instance's schema hint and archive member selector travel with the
// download so the core parser applies the same parse contract as the
// configured source. Failures are reported through the completion result
// instead of being retried against a different source.
class SkyCatalogImportWorkflow final {
public:
    using StatusHandler = std::function<void(const QString&)>;
    using SourceCompletionHandler = std::function<void(SkyCatalogSourceImportResult)>;
    using ConstellationCompletionHandler = std::function<void(SkyConstellationLineImportResult)>;

public:
    explicit SkyCatalogImportWorkflow(QNetworkAccessManager* networkAccessManager);
    ~SkyCatalogImportWorkflow();

    [[nodiscard]] bool isAvailable() const noexcept;

    void downloadSource(
        const SkyCatalogSourceInstance& source,
        skygate::ephemeris::CatalogCompositionPolicy policy,
        QObject* callbackContext,
        StatusHandler statusHandler,
        SourceCompletionHandler completionHandler
    ) const;

    void downloadConstellationLines(
        const QStringList& urlTexts,
        QObject* callbackContext,
        StatusHandler statusHandler,
        ConstellationCompletionHandler completionHandler
    ) const;

private:
    std::unique_ptr<CatalogCoordinator> m_catalogCoordinator;
};

}  // namespace skygate::ui::internal
