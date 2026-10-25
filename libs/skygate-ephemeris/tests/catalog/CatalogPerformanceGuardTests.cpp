#include "BaseCelestialBody.hpp"
#include "CatalogArchiveTestSupport.hpp"
#include "CatalogMergeTestSupport.hpp"
#include "DistantCelestialBody.hpp"
#include "catalog/CatalogIdentifier.hpp"
#include "catalog/CatalogPayloadParser.hpp"
#include "skygate/testsupport/PerformanceBudget.hpp"
#include "catalog/io/CatalogZipEntrySelector.hpp"

#include <QtTest/QtTest>

#include <QElapsedTimer>

#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using skygate::ephemeris::BaseCelestialBody;
using skygate::ephemeris::CatalogIdentifier;
using skygate::ephemeris::DistantCelestialBody;
using skygate::ephemeris::tests::CatalogMergeTestSupport;
using skygate::ephemeris::tests::makeZip;
using skygate::ephemeris::tests::ZipEntrySpec;

class CatalogPerformanceGuardTests final : public QObject {
    Q_OBJECT

private slots:
    void parsesModeratelyLargeHygCatalogWithinGuardrail();
    void parsesLargeOpenNgcCatalogWithinGuardrail();
    void scansZipWithManyEntriesWithinGuardrail();
    void mergesLargeFixtureWithoutAllPairsScan();
    void wallClockBudgetsAreAdvisoryByDefault();
};

namespace {

constexpr qint64 kLargeCatalogParseBudgetMs = 15000;
constexpr qint64 kZipScanBudgetMs = 5000;

void verifyElapsedBelow(const qint64 elapsedMs, const qint64 budgetMs, const char* operationName)
{
    const skygate::testsupport::PerformanceBudgetDecision decision =
        skygate::testsupport::reportPerformanceBudget(elapsedMs, budgetMs, operationName);
    QVERIFY2(
        decision != skygate::testsupport::PerformanceBudgetDecision::StrictOverBudget,
        qPrintable(
            skygate::testsupport::strictPerformanceBudgetHint(
                skygate::testsupport::performanceMetricMessage(elapsedMs, budgetMs, operationName, true)
            )
        )
    );
}

std::string makeLargeHygCatalog(const int rowCount)
{
    std::string csv = "id,hip,proper,ra,dec,mag,bf,con\n";
    csv.reserve(static_cast<std::size_t>(rowCount) * 48U);
    for (int index = 0; index < rowCount; ++index) {
        const double rightAscensionHours = std::fmod(index * 0.037, 24.0);
        const double declinationDeg = -70.0 + static_cast<double>(index % 140);
        const double magnitude = 2.0 + static_cast<double>(index % 80) / 10.0;
        csv += std::to_string(index + 1);
        csv += ',';
        csv += std::to_string(100000 + index);
        csv += ",Guard Star ";
        csv += std::to_string(index + 1);
        csv += ',';
        csv += std::to_string(rightAscensionHours);
        csv += ',';
        csv += std::to_string(declinationDeg);
        csv += ',';
        csv += std::to_string(magnitude);
        csv += ",,Tst\n";
    }
    return csv;
}

std::string makeLargeOpenNgcCatalog(const int rowCount)
{
    std::string csv = "Name;Type;RA;Dec;Const;MajAx;MinAx;PosAng;B-Mag;V-Mag;J-Mag;H-Mag;K-Mag;"
                      "SurfBr;Hubble;Cstar U-Mag;Cstar B-Mag;Cstar V-Mag;M;NGC;IC;Cstar Names;"
                      "Identifiers;Common names;NED notes;OpenNGC notes\n";
    csv.reserve(static_cast<std::size_t>(rowCount) * 96U);
    for (int index = 0; index < rowCount; ++index) {
        const int hour = (index / 3600) % 24;
        const int minute = (index / 60) % 60;
        const int second = index % 60;
        const int decDeg = (index % 120) - 60;
        const int absDecDeg = std::abs(decDeg);
        const char sign = decDeg < 0 ? '-' : '+';
        const int ngcId = 100000 + index;
        csv += "NGC";
        csv += std::to_string(ngcId);
        csv += ";G;";
        csv += QStringLiteral("%1:%2:%3.00")
                   .arg(hour, 2, 10, QLatin1Char('0'))
                   .arg(minute, 2, 10, QLatin1Char('0'))
                   .arg(second, 2, 10, QLatin1Char('0'))
                   .toStdString();
        csv += ';';
        csv += QStringLiteral("%1%2:00:00.0").arg(QChar(sign)).arg(absDecDeg, 2, 10, QLatin1Char('0')).toStdString();
        csv += ";Tst;1.5;1.0;0;;";
        csv += std::to_string(8.0 + static_cast<double>(index % 50) / 10.0);
        csv += ";;;;;;;;;";
        csv += std::to_string(ngcId);
        csv += ";;;PGC ";
        csv += std::to_string(ngcId);
        csv += ";Guard Galaxy ";
        csv += std::to_string(index + 1);
        csv += ";;\n";
    }
    return csv;
}

std::string_view payloadView(const std::string& payload)
{
    return std::string_view(payload.data(), payload.size());
}

}  // namespace

void CatalogPerformanceGuardTests::parsesModeratelyLargeHygCatalogWithinGuardrail()
{
    const std::string payload = makeLargeHygCatalog(20000);
    const skygate::ephemeris::CatalogPayloadParser parser;

    QElapsedTimer timer;
    timer.start();
    const auto result = parser.parseResult(payloadView(payload));
    const qint64 elapsedMs = timer.elapsed();

    QVERIFY2(result.isSuccess(), result.errorDetail.c_str());
    QVERIFY(result.catalog != nullptr);
    QCOMPARE(result.diagnostics.parsedBodyCount, 20000U);
    verifyElapsedBelow(elapsedMs, kLargeCatalogParseBudgetMs, "large HYG parse");
}

void CatalogPerformanceGuardTests::parsesLargeOpenNgcCatalogWithinGuardrail()
{
    const std::string payload = makeLargeOpenNgcCatalog(12000);
    const skygate::ephemeris::CatalogPayloadParser parser;

    QElapsedTimer timer;
    timer.start();
    const auto result = parser.parseResult(payloadView(payload));
    const qint64 elapsedMs = timer.elapsed();

    QVERIFY2(result.isSuccess(), result.errorDetail.c_str());
    QVERIFY(result.catalog != nullptr);
    QCOMPARE(result.diagnostics.parsedBodyCount, 12000U);
    verifyElapsedBelow(elapsedMs, kLargeCatalogParseBudgetMs, "large OpenNGC parse");
}

void CatalogPerformanceGuardTests::scansZipWithManyEntriesWithinGuardrail()
{
    std::vector<ZipEntrySpec> entries;
    entries.reserve(3001U);
    for (int index = 0; index < 3000; ++index) {
        entries.push_back(
            ZipEntrySpec{.path = "notes/readme-" + std::to_string(index) + ".txt", .data = "not a catalog\n"}
        );
    }
    entries.push_back(
        ZipEntrySpec{.path = "catalogs/hyg.csv", .data = "id,hip,proper,ra,dec,mag\n1,42,Target,1.0,2.0,3.0\n"}
    );
    const std::string zipData = makeZip(entries);

    QElapsedTimer timer;
    timer.start();
    const skygate::ephemeris::CatalogZipEntrySelection extracted =
        skygate::ephemeris::CatalogZipEntrySelector::select(zipData, std::nullopt);
    const qint64 elapsedMs = timer.elapsed();

    QVERIFY(extracted.status == skygate::ephemeris::CatalogZipEntrySelection::Status::Selected);
    QVERIFY(extracted.payload.find("Target") != std::string::npos);
    verifyElapsedBelow(elapsedMs, kZipScanBudgetMs, "large ZIP entry scan");
}

void CatalogPerformanceGuardTests::mergesLargeFixtureWithoutAllPairsScan()
{
    constexpr std::size_t kFixtureSize = 6000;

    std::vector<DistantCelestialBody> primaryDso;
    std::vector<DistantCelestialBody> deepSkyDso;
    primaryDso.reserve(kFixtureSize);
    deepSkyDso.reserve(kFixtureSize);

    for (std::size_t index = 0; index < kFixtureSize; ++index) {
        const std::string ngc = std::to_string(100000 + index);
        primaryDso.push_back(
            CatalogMergeTestSupport::makeDeepSkyObject(
                "primary_" + ngc, "Primary " + ngc, {"NGC " + ngc}, {CatalogIdentifier::make("ngc", ngc)}, 1.0, 2.0
            )
        );
        deepSkyDso.push_back(
            CatalogMergeTestSupport::makeDeepSkyObject(
                "secondary_" + ngc,
                "Secondary " + ngc,
                {"NGC " + ngc},
                {CatalogIdentifier::make("ngc", ngc), CatalogIdentifier::make("ic", ngc)},
                1.0,
                2.0
            )
        );
    }

    const auto primary = CatalogMergeTestSupport::createCatalog({}, std::move(primaryDso));
    const auto deepSky = CatalogMergeTestSupport::createCatalog({}, std::move(deepSkyDso));
    QVERIFY(primary != nullptr);
    QVERIFY(deepSky != nullptr);

    const auto result = CatalogMergeTestSupport::composePrimaryWithDeepSky(*primary, *deepSky);

    QVERIFY(result.isSuccess());
    QCOMPARE(result.deepSkyObjectCount, kFixtureSize);

    const BaseCelestialBody* first =
        CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "secondary_100000");
    QVERIFY(first != nullptr);
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*first, "ngc", "100000"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*first, "ic", "100000"));

    const BaseCelestialBody* last = CatalogMergeTestSupport::findBodyById(result.catalog->bodies(), "secondary_105999");
    QVERIFY(last != nullptr);
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*last, "ngc", "105999"));
    QVERIFY(CatalogMergeTestSupport::hasIdentifier(*last, "ic", "105999"));

    QCOMPARE(CatalogMergeTestSupport::countBodiesById(result.catalog->bodies(), "primary_100000"), std::size_t{0});
}

void CatalogPerformanceGuardTests::wallClockBudgetsAreAdvisoryByDefault()
{
    using skygate::testsupport::PerformanceBudgetDecision;

    QCOMPARE(
        skygate::testsupport::performanceBudgetDecision(200, 100, false), PerformanceBudgetDecision::AdvisoryOverBudget
    );
    QCOMPARE(
        skygate::testsupport::performanceBudgetDecision(200, 100, true), PerformanceBudgetDecision::StrictOverBudget
    );
    QCOMPARE(skygate::testsupport::performanceBudgetDecision(99, 100, true), PerformanceBudgetDecision::WithinBudget);
}

QTEST_APPLESS_MAIN(CatalogPerformanceGuardTests)

#include "CatalogPerformanceGuardTests.moc"
