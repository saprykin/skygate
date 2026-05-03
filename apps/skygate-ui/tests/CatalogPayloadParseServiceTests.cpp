#include "catalog/CatalogPayloadParseService.hpp"

#include <QtTest/QtTest>

#include <QEventLoop>
#include <QThreadPool>
#include <QTimer>

#include <utility>

namespace {

QByteArray validPayload()
{
    return QByteArray(
        "id,hip,proper,ra,dec,mag\n"
        "1,42,Sirius,6.7525,-16.7161,-1.46\n"
    );
}

template <typename StartFn>
void runAsync(StartFn startFn)
{
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(5000);
    startFn(loop);
    loop.exec();
    const bool timedOut = !timeout.isActive();
    timeout.stop();
    QVERIFY(!timedOut);
}

}  // namespace

class CatalogPayloadParseServiceTests final : public QObject {
    Q_OBJECT

private slots:
    void parsesValidPayloadAndReportsProgress();
    void invalidPayloadCompletesWithFailure();
    void destroyedContextSuppressesCallbacks();
};

void CatalogPayloadParseServiceTests::parsesValidPayloadAndReportsProgress()
{
    CatalogPayloadParseService service;
    std::size_t lastProgress = 0U;
    skygate::ephemeris::CatalogLoadResult finalResult;

    runAsync([&](QEventLoop& loop) {
        service.parseAsync(
            validPayload(),
            this,
            {},
            [&lastProgress](const std::size_t parsedObjectCount) {
                lastProgress = parsedObjectCount;
            },
            [&finalResult, &loop](skygate::ephemeris::CatalogLoadResult result) {
                finalResult = std::move(result);
                loop.quit();
            }
        );
    });

    QVERIFY(finalResult.isSuccess());
    QCOMPARE(finalResult.diagnostics.parsedBodyCount, 1U);
    QCOMPARE(lastProgress, 1U);
}

void CatalogPayloadParseServiceTests::invalidPayloadCompletesWithFailure()
{
    CatalogPayloadParseService service;
    skygate::ephemeris::CatalogLoadResult finalResult;

    runAsync([&](QEventLoop& loop) {
        service.parseAsync(
            "not a catalog",
            this,
            {},
            {},
            [&finalResult, &loop](skygate::ephemeris::CatalogLoadResult result) {
                finalResult = std::move(result);
                loop.quit();
            }
        );
    });

    QVERIFY(!finalResult.isSuccess());
}

void CatalogPayloadParseServiceTests::destroyedContextSuppressesCallbacks()
{
    CatalogPayloadParseService service;
    bool progressCalled = false;
    bool completionCalled = false;
    auto* context = new QObject();

    service.parseAsync(
        validPayload(),
        context,
        {},
        [&progressCalled](std::size_t) {
            progressCalled = true;
        },
        [&completionCalled](skygate::ephemeris::CatalogLoadResult) {
            completionCalled = true;
        }
    );
    delete context;

    QVERIFY(QThreadPool::globalInstance()->waitForDone(5000));
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    QVERIFY(!progressCalled);
    QVERIFY(!completionCalled);
}

QTEST_GUILESS_MAIN(CatalogPayloadParseServiceTests)

#include "CatalogPayloadParseServiceTests.moc"
