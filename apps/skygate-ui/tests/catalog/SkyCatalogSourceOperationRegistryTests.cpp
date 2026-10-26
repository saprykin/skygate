#include "catalog/CatalogDownloadCancellation.hpp"
#include "catalog/SkyCatalogSourceInstance.hpp"
#include "catalog/SkyCatalogSourceOperation.hpp"
#include "catalog/SkyCatalogSourceOperationRegistry.hpp"

#include <QtTest/QtTest>

#include <cstdint>
#include <memory>

using skygate::ephemeris::CatalogCompositionPolicy;
using skygate::ui::internal::SkyCatalogSourceInstance;
using skygate::ui::internal::SkyCatalogSourceOperation;
using skygate::ui::internal::SkyCatalogSourceOperationRegistry;

namespace {

SkyCatalogSourceInstance customSource(const QString& instanceId)
{
    SkyCatalogSourceInstance instance =
        SkyCatalogSourceInstance::createCustom(QStringLiteral("https://example.invalid/catalog.csv"));
    instance.instanceId = instanceId;
    return instance;
}

}  // namespace

class SkyCatalogSourceOperationRegistryTests final : public QObject {
    Q_OBJECT

private slots:
    void upsertAllocatesARevisionPerInstance();
    void upsertKeepsTheActiveIncarnation();
    void findReportsOnlyActiveInstances();
    void isCurrentMatchesTheHandedOutRevision();
    void removeRetiresTheIncarnationBeforeErasing();
    void retireKeepsTheRecordAndSupersedesItsWork();
    void retireAllSupersedesEveryPendingInstance();
    void tracksTheActiveDownloadOwner();
};

void SkyCatalogSourceOperationRegistryTests::upsertAllocatesARevisionPerInstance()
{
    SkyCatalogSourceOperationRegistry registry;

    const std::uint64_t firstRevision = registry.upsert(customSource("a"), CatalogCompositionPolicy::Merge)->revision();
    const std::uint64_t secondRevision =
        registry.upsert(customSource("b"), CatalogCompositionPolicy::DeepSkyOnly)->revision();

    QCOMPARE(registry.operations().size(), qsizetype{2});
    QVERIFY(firstRevision != secondRevision);
    QVERIFY(firstRevision != 0U);
    QVERIFY(secondRevision != 0U);

    // Requesting the same instance again keeps its record and its revision.
    const std::uint64_t repeatedRevision =
        registry.upsert(customSource("a"), CatalogCompositionPolicy::Merge)->revision();
    QCOMPARE(repeatedRevision, firstRevision);
    QCOMPARE(registry.operations().size(), qsizetype{2});
}

void SkyCatalogSourceOperationRegistryTests::upsertKeepsTheActiveIncarnation()
{
    SkyCatalogSourceOperationRegistry registry;
    SkyCatalogSourceOperation* operation = registry.upsert(customSource("a"), CatalogCompositionPolicy::Merge);
    operation->beginWork(QStringLiteral("Downloading..."), nullptr);
    operation->accept(QByteArrayLiteral("payload"));
    const std::uint64_t acceptedRevision = operation->revision();

    SkyCatalogSourceInstance updated = customSource("a");
    updated.version = QStringLiteral("v2");
    operation = registry.upsert(updated, CatalogCompositionPolicy::DeepSkyOnly);

    // A new request replaces the attempt only; the accepted facts and the
    // revision of the incarnation stay in place.
    QCOMPARE(registry.operations().size(), qsizetype{1});
    QCOMPARE(operation->revision(), acceptedRevision);
    QCOMPARE(operation->attempt().version, QStringLiteral("v2"));
    QCOMPARE(operation->policy(), CatalogCompositionPolicy::DeepSkyOnly);
    QVERIFY(operation->hasAcceptedFacts());
    QCOMPARE(operation->acceptedPayload(), QByteArrayLiteral("payload"));
}

void SkyCatalogSourceOperationRegistryTests::findReportsOnlyActiveInstances()
{
    SkyCatalogSourceOperationRegistry registry;
    QVERIFY(registry.find(QStringLiteral("missing")) == nullptr);

    QVERIFY(registry.upsert(customSource("a"), CatalogCompositionPolicy::Merge) != nullptr);
    QVERIFY(registry.find(QStringLiteral("a")) != nullptr);
    QVERIFY(registry.find(QStringLiteral("b")) == nullptr);

    const SkyCatalogSourceOperationRegistry& readOnly = registry;
    QVERIFY(readOnly.find(QStringLiteral("a")) != nullptr);
    QVERIFY(readOnly.find(QStringLiteral("b")) == nullptr);
}

void SkyCatalogSourceOperationRegistryTests::isCurrentMatchesTheHandedOutRevision()
{
    SkyCatalogSourceOperationRegistry registry;
    const std::uint64_t revision = registry.upsert(customSource("a"), CatalogCompositionPolicy::Merge)->revision();

    QVERIFY(registry.isCurrent(QStringLiteral("a"), revision));
    QVERIFY(!registry.isCurrent(QStringLiteral("a"), revision + 1U));
    QVERIFY(!registry.isCurrent(QStringLiteral("missing"), revision));
    QVERIFY(!registry.isCurrent(QStringLiteral("missing"), 0U));
}

void SkyCatalogSourceOperationRegistryTests::removeRetiresTheIncarnationBeforeErasing()
{
    SkyCatalogSourceOperationRegistry registry;
    const auto cancellation = std::make_shared<CatalogDownloadCancellation>();
    SkyCatalogSourceOperation* operation = registry.upsert(customSource("a"), CatalogCompositionPolicy::Merge);
    operation->beginWork(QStringLiteral("Downloading..."), cancellation);
    const std::uint64_t removedRevision = operation->revision();

    registry.remove(QStringLiteral("a"));

    QVERIFY(cancellation->isCanceled());
    QVERIFY(registry.find(QStringLiteral("a")) == nullptr);
    QVERIFY(registry.operations().isEmpty());
    QVERIFY(!registry.isCurrent(QStringLiteral("a"), removedRevision));

    // A later instance with the same ID starts from a fresh revision instead of
    // inheriting the removed one.
    const std::uint64_t readdedRevision =
        registry.upsert(customSource("a"), CatalogCompositionPolicy::Merge)->revision();
    QVERIFY(readdedRevision > removedRevision);
    QVERIFY(registry.isCurrent(QStringLiteral("a"), readdedRevision));
}

void SkyCatalogSourceOperationRegistryTests::retireKeepsTheRecordAndSupersedesItsWork()
{
    SkyCatalogSourceOperationRegistry registry;
    const auto catalogCancellation = std::make_shared<CatalogDownloadCancellation>();
    const auto relatedCancellation = std::make_shared<CatalogDownloadCancellation>();
    SkyCatalogSourceOperation* operation = registry.upsert(customSource("a"), CatalogCompositionPolicy::Merge);
    operation->beginWork(QStringLiteral("Downloading..."), catalogCancellation);
    operation->beginRelatedWork(relatedCancellation);
    const std::uint64_t retiredRevision = operation->revision();
    QVERIFY(registry.hasPendingRelatedWork());

    registry.retire(QStringLiteral("a"));

    QVERIFY(catalogCancellation->isCanceled());
    QVERIFY(relatedCancellation->isCanceled());
    QVERIFY(!registry.hasPendingRelatedWork());
    operation = registry.find(QStringLiteral("a"));
    QVERIFY(operation != nullptr);
    QVERIFY(operation->revision() > retiredRevision);
    QVERIFY(!registry.isCurrent(QStringLiteral("a"), retiredRevision));
    QVERIFY(registry.isCurrent(QStringLiteral("a"), operation->revision()));

    // Retiring an instance that holds no record leaves the collection alone.
    registry.retire(QStringLiteral("missing"));
    QCOMPARE(registry.operations().size(), qsizetype{1});
}

void SkyCatalogSourceOperationRegistryTests::retireAllSupersedesEveryPendingInstance()
{
    SkyCatalogSourceOperationRegistry registry;
    const auto firstCancellation = std::make_shared<CatalogDownloadCancellation>();
    const auto secondCancellation = std::make_shared<CatalogDownloadCancellation>();
    const std::uint64_t firstRevision = registry.upsert(customSource("a"), CatalogCompositionPolicy::Merge)->revision();
    const std::uint64_t secondRevision =
        registry.upsert(customSource("b"), CatalogCompositionPolicy::DeepSkyOnly)->revision();
    registry.find(QStringLiteral("a"))->beginWork(QStringLiteral("Downloading..."), firstCancellation);
    registry.find(QStringLiteral("b"))->beginRelatedWork(secondCancellation);

    registry.retireAll();

    QVERIFY(firstCancellation->isCanceled());
    QVERIFY(secondCancellation->isCanceled());
    QVERIFY(!registry.hasPendingRelatedWork());
    QVERIFY(!registry.isCurrent(QStringLiteral("a"), firstRevision));
    QVERIFY(!registry.isCurrent(QStringLiteral("b"), secondRevision));
    QVERIFY(registry.isCurrent(QStringLiteral("a"), registry.find(QStringLiteral("a"))->revision()));
    QVERIFY(registry.isCurrent(QStringLiteral("b"), registry.find(QStringLiteral("b"))->revision()));
}

void SkyCatalogSourceOperationRegistryTests::tracksTheActiveDownloadOwner()
{
    SkyCatalogSourceOperationRegistry registry;
    QVERIFY(!registry.hasActiveDownload());
    QVERIFY(registry.activeDownloadInstanceId().isEmpty());

    registry.setActiveDownloadInstanceId(QStringLiteral("a"));
    QVERIFY(registry.hasActiveDownload());
    QCOMPARE(registry.activeDownloadInstanceId(), QStringLiteral("a"));

    // A completion of a superseded instance leaves its successor's identity.
    registry.endActiveDownload(QStringLiteral("b"));
    QCOMPARE(registry.activeDownloadInstanceId(), QStringLiteral("a"));

    registry.endActiveDownload(QStringLiteral("a"));
    QVERIFY(!registry.hasActiveDownload());

    registry.setActiveDownloadInstanceId(QStringLiteral("a"));
    registry.clearActiveDownload();
    QVERIFY(!registry.hasActiveDownload());
}

QTEST_APPLESS_MAIN(SkyCatalogSourceOperationRegistryTests)

#include "SkyCatalogSourceOperationRegistryTests.moc"
