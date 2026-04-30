#pragma once

#include "skygate/ephemeris/CatalogLoadResult.hpp"
#include "skygate/ephemeris/ConstellationData.hpp"
#include "skygate/ephemeris/IStarCatalog.hpp"

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

struct SkyCatalogImportResult final {
    QByteArray payload;
    std::unique_ptr<skygate::ephemeris::IStarCatalog> catalog;
    skygate::ephemeris::CatalogLoadDiagnostics diagnostics;
    QString sourceLabel;
    QString errorText;
};

struct SkyDeepSkyCatalogImportResult final {
    QByteArray payload;
    std::unique_ptr<skygate::ephemeris::IStarCatalog> catalog;
    std::size_t foundObjectCount = 0;
    QString sourceLabel;
    QString errorText;
};

struct SkyConstellationLineImportResult final {
    std::vector<skygate::ephemeris::ConstellationLineRef> lineRefs;
    std::vector<skygate::ephemeris::ConstellationLabelRef> labelRefs;
    std::size_t constellationCount = 0;
    QString statusSuffix;

    [[nodiscard]] bool hasCustomLines() const noexcept;
};

class SkyCatalogImportWorkflow final {
public:
    using StatusHandler = std::function<void(const QString&)>;
    using CatalogCompletionHandler = std::function<void(SkyCatalogImportResult)>;
    using DeepSkyCompletionHandler = std::function<void(SkyDeepSkyCatalogImportResult)>;
    using ConstellationCompletionHandler =
        std::function<void(SkyConstellationLineImportResult)>;

public:
    explicit SkyCatalogImportWorkflow(QNetworkAccessManager* networkAccessManager);
    ~SkyCatalogImportWorkflow();

    [[nodiscard]] bool isAvailable() const noexcept;

    void downloadCatalog(
        const QStringList& urlTexts,
        const QString& sourceLabel,
        QObject* callbackContext,
        StatusHandler statusHandler,
        CatalogCompletionHandler completionHandler
    ) const;

    void downloadDeepSkyCatalog(
        const QStringList& urlTexts,
        const QString& sourceLabel,
        QObject* callbackContext,
        StatusHandler statusHandler,
        DeepSkyCompletionHandler completionHandler
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
