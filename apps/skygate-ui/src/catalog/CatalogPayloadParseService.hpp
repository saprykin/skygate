#pragma once

#include "CatalogParseOptions.hpp"

#include "catalog/CatalogLoadResult.hpp"
#include "catalog/IStarCatalog.hpp"

#include <QByteArray>

#include <cstddef>
#include <functional>
#include <memory>

class QObject;

class CatalogPayloadParseService final {
public:
    using ProgressHandler = std::function<void(std::size_t parsedObjectCount)>;
    using CompletionHandler = std::function<void(skygate::ephemeris::CatalogLoadResult)>;

public:
    void parseAsync(
        QByteArray payload,
        QObject* callbackContext,
        CatalogParseOptions parseOptions,
        ProgressHandler progressHandler,
        CompletionHandler completionHandler
    ) const;
};
