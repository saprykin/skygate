#include "CatalogPayloadParseService.hpp"

#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QThreadPool>

#include "skygate/ephemeris/CatalogPayloadParser.hpp"

#include <memory>
#include <string_view>
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
    auto task = QRunnable::create([
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
        const auto parsedResult = std::make_shared<skygate::ephemeris::CatalogLoadResult>(
            parser.parseResult(payloadView, reportProgress, selectionOptions)
        );

        QObject* const contextObject = safeContext.data();
        if (contextObject == nullptr) {
            return;
        }

        QMetaObject::invokeMethod(
            contextObject,
            [completionHandler, parsedResult]() mutable {
                completionHandler(std::move(*parsedResult));
            },
            Qt::QueuedConnection
        );
    });
    task->setAutoDelete(true);
    QThreadPool::globalInstance()->start(task);
}
