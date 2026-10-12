#pragma once

#include "SkyCatalogSourceImportResult.hpp"
#include "SkyCatalogSourceInstance.hpp"
#include "SkyConstellationLineImportResult.hpp"

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

class CatalogDownloadCancellation;
class QNetworkAccessManager;
class QObject;
class CatalogCoordinator;

namespace skygate::ui::internal {

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

    // Starts the download and returns the handle that cancels it, or null
    // when no download was started. A canceled download stops without
    // reporting a completion; an ordinary failure is reported through the
    // completion result.
    std::shared_ptr<CatalogDownloadCancellation> downloadSource(
        const SkyCatalogSourceInstance& source,
        skygate::ephemeris::CatalogCompositionPolicy policy,
        QObject* callbackContext,
        StatusHandler statusHandler,
        SourceCompletionHandler completionHandler
    ) const;

    std::shared_ptr<CatalogDownloadCancellation> downloadConstellationLines(
        const QStringList& urlTexts,
        QObject* callbackContext,
        StatusHandler statusHandler,
        ConstellationCompletionHandler completionHandler
    ) const;

private:
    std::unique_ptr<CatalogCoordinator> m_catalogCoordinator;
};

}  // namespace skygate::ui::internal
