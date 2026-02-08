#pragma once

#include "skygate/ephemeris/IStarCatalog.hpp"

#include <QByteArray>

#include <cstddef>
#include <functional>
#include <memory>

class QObject;

class CatalogPayloadParseService final {
public:
    using ProgressHandler = std::function<void(std::size_t parsedObjectCount)>;
    using CompletionHandler = std::function<void(std::unique_ptr<skygate::ephemeris::IStarCatalog>)>;

public:
    void parseAsync(
        QByteArray payload,
        QObject* callbackContext,
        ProgressHandler progressHandler,
        CompletionHandler completionHandler
    ) const;
};
