#include "CatalogPayloadParseService.hpp"

#include <QMetaObject>
#include <QObject>
#include <QPointer>

#include "skygate/ephemeris/CatalogPayloadParser.hpp"

#include <string_view>
#include <thread>
#include <utility>

void CatalogPayloadParseService::parseAsync(
    QByteArray payload,
    QObject* callbackContext,
    ProgressHandler progressHandler,
    CompletionHandler completionHandler
) const
{
    QPointer<QObject> safeContext(callbackContext);
    std::thread([
        payload = std::move(payload),
        safeContext,
        progressHandler = std::move(progressHandler),
        completionHandler = std::move(completionHandler)
    ]() mutable {
        const auto reportProgress = [&safeContext, &progressHandler](const std::size_t parsedObjectCount) {
            if (!progressHandler) {
                return;
            }

            QObject* const contextObject = safeContext.data();
            if (contextObject == nullptr) {
                return;
            }

            QMetaObject::invokeMethod(
                contextObject,
                [progressHandler, parsedObjectCount]() mutable {
                    progressHandler(parsedObjectCount);
                },
                Qt::QueuedConnection
            );
        };

        const std::string_view payloadView(
            payload.constData(),
            static_cast<std::size_t>(payload.size())
        );
        const skygate::ephemeris::CatalogPayloadParser parser;
        std::unique_ptr<skygate::ephemeris::IStarCatalog> parsedCatalog = parser.parse(
            payloadView,
            reportProgress
        );

        QObject* const contextObject = safeContext.data();
        if (contextObject == nullptr) {
            return;
        }

        auto* parsedCatalogRaw = parsedCatalog.release();
        const bool scheduled = QMetaObject::invokeMethod(
            contextObject,
            [completionHandler = std::move(completionHandler), parsedCatalogRaw]() mutable {
                std::unique_ptr<skygate::ephemeris::IStarCatalog> parsedCatalogGuard(parsedCatalogRaw);
                completionHandler(std::move(parsedCatalogGuard));
            },
            Qt::QueuedConnection
        );
        if (!scheduled) {
            delete parsedCatalogRaw;
        }
    }).detach();
}
