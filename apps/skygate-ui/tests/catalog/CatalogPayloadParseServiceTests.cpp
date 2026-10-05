#include "catalog/CatalogPayloadParseService.hpp"
#include "BaseCelestialBody.hpp"
#include "CatalogParseOptions.hpp"
#include "CatalogTestPayloads.hpp"

#include <QtTest/QtTest>

#include <QEventLoop>
#include <QThreadPool>
#include <QTimer>

#include <cstddef>
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
    void appliesArchiveMemberAndSchemaHintToRequest();
    void reportsMissingMemberAndContradictoryHint();
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
            [&lastProgress](const std::size_t parsedObjectCount) { lastProgress = parsedObjectCount; },
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

    QTest::ignoreMessage(QtWarningMsg, "Catalog payload parse failed: Catalog payload format is not recognized.");
    runAsync([&](QEventLoop& loop) {
        service.parseAsync(
            "not a catalog", this, {}, {}, [&finalResult, &loop](skygate::ephemeris::CatalogLoadResult result) {
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
        [&progressCalled](std::size_t) { progressCalled = true; },
        [&completionCalled](skygate::ephemeris::CatalogLoadResult) { completionCalled = true; }
    );
    delete context;

    QVERIFY(QThreadPool::globalInstance()->waitForDone(5000));
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    QVERIFY(!progressCalled);
    QVERIFY(!completionCalled);
}

void CatalogPayloadParseServiceTests::appliesArchiveMemberAndSchemaHintToRequest()
{
    CatalogPayloadParseService service;
    skygate::ephemeris::CatalogLoadResult finalResult;

    runAsync([&](QEventLoop& loop) {
        service.parseAsync(
            skygate::ui::tests::sampleTwoMemberCatalogZip(),
            this,
            CatalogParseOptions{
                .archiveMember = QStringLiteral("catalog/hyg.csv"),
                .schemaHint = skygate::ephemeris::CatalogSourceType::HygCsv,
            },
            {},
            [&finalResult, &loop](skygate::ephemeris::CatalogLoadResult result) {
                finalResult = std::move(result);
                loop.quit();
            }
        );
    });

    QVERIFY(finalResult.isSuccess());
    const auto bodies = finalResult.catalog->bodies();
    QCOMPARE(bodies.size(), std::size_t{1});
    QCOMPARE(QString::fromStdString(bodies.front()->id), QStringLiteral("hip_42"));
}

void CatalogPayloadParseServiceTests::reportsMissingMemberAndContradictoryHint()
{
    CatalogPayloadParseService service;

    QTest::ignoreMessage(
        QtWarningMsg, "Catalog ZIP parse failed: ZIP catalog payload does not contain member 'catalog/missing.csv'."
    );
    skygate::ephemeris::CatalogLoadResult missingMemberResult;
    runAsync([&](QEventLoop& loop) {
        service.parseAsync(
            skygate::ui::tests::sampleTwoMemberCatalogZip(),
            this,
            CatalogParseOptions{.archiveMember = QStringLiteral("catalog/missing.csv")},
            {},
            [&missingMemberResult, &loop](skygate::ephemeris::CatalogLoadResult result) {
                missingMemberResult = std::move(result);
                loop.quit();
            }
        );
    });
    QVERIFY(!missingMemberResult.isSuccess());
    QCOMPARE(missingMemberResult.errorCode, skygate::ephemeris::CatalogLoadResult::ErrorCode::ArchiveMemberNotFound);
    QVERIFY(QString::fromStdString(missingMemberResult.errorDetail).contains(QStringLiteral("catalog/missing.csv")));

    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog payload parse failed: Catalog payload schema 'HYG CSV' does not match the expected schema hint "
        "'OpenNGC CSV'."
    );
    skygate::ephemeris::CatalogLoadResult contradictoryHintResult;
    runAsync([&](QEventLoop& loop) {
        service.parseAsync(
            validPayload(),
            this,
            CatalogParseOptions{.schemaHint = skygate::ephemeris::CatalogSourceType::OpenNgcCsv},
            {},
            [&contradictoryHintResult, &loop](skygate::ephemeris::CatalogLoadResult result) {
                contradictoryHintResult = std::move(result);
                loop.quit();
            }
        );
    });
    QVERIFY(!contradictoryHintResult.isSuccess());
    QCOMPARE(contradictoryHintResult.errorCode, skygate::ephemeris::CatalogLoadResult::ErrorCode::SchemaHintMismatch);
}

QTEST_GUILESS_MAIN(CatalogPayloadParseServiceTests)

#include "CatalogPayloadParseServiceTests.moc"
