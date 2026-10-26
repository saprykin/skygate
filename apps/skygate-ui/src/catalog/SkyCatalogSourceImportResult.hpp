#pragma once

#include "catalog/CatalogLoadResult.hpp"
#include "catalog/IStarCatalog.hpp"

#include <QByteArray>
#include <QString>

#include <cstddef>
#include <memory>

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

}  // namespace skygate::ui::internal
