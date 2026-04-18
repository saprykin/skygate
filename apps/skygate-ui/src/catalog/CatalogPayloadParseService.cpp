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
    skygate::ephemeris::CatalogSelectionOptions selectionOptions,
    ProgressHandler progressHandler,
    CompletionHandler completionHandler
) const
{
    QPointer<QObject> safeContext(callbackContext);
    std::thread([
        payload = std::move(payload),
        safeContext,
        selectionOptions,
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
        auto* parsedResultRaw = new skygate::ephemeris::CatalogLoadResult(parser.parseResult(
            payloadView,
            reportProgress,
            selectionOptions
        ));

        QObject* const contextObject = safeContext.data();
        if (contextObject == nullptr) {
            delete parsedResultRaw;
            return;
        }

        const bool scheduled = QMetaObject::invokeMethod(
            contextObject,
            [completionHandler = std::move(completionHandler), parsedResultRaw]() mutable {
                std::unique_ptr<skygate::ephemeris::CatalogLoadResult> parsedResultGuard(parsedResultRaw);
                completionHandler(std::move(*parsedResultGuard));
            },
            Qt::QueuedConnection
        );
        if (!scheduled) {
            delete parsedResultRaw;
        }
    }).detach();
}
