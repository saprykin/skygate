#include "skygate/ephemeris/CatalogPayloadParser.hpp"

#include <QtTest/QtTest>

#include <QElapsedTimer>

#include <cmath>
#include <cstdlib>
#include <string>
#include <string_view>

class CatalogPerformanceGuardTests final : public QObject {
    Q_OBJECT

private slots:
    void parsesModeratelyLargeHygCatalogWithinGuardrail();
    void parsesModeratelyLargeOpenNgcCatalogWithinGuardrail();
};

namespace {

constexpr qint64 kLargeCatalogParseBudgetMs = 15000;

void verifyElapsedBelow(
    const qint64 elapsedMs,
    const qint64 budgetMs,
    const char* operationName
)
{
    QVERIFY2(
        elapsedMs < budgetMs,
        qPrintable(QStringLiteral("%1 took %2 ms, budget is %3 ms")
            .arg(QString::fromUtf8(operationName))
            .arg(elapsedMs)
            .arg(budgetMs))
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
    std::string csv =
        "Name;Type;RA;Dec;Const;MajAx;MinAx;PosAng;B-Mag;V-Mag;J-Mag;H-Mag;K-Mag;"
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
        csv += QStringLiteral("%1%2:00:00.0")
            .arg(QChar(sign))
            .arg(absDecDeg, 2, 10, QLatin1Char('0'))
            .toStdString();
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

void CatalogPerformanceGuardTests::parsesModeratelyLargeOpenNgcCatalogWithinGuardrail()
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

QTEST_APPLESS_MAIN(CatalogPerformanceGuardTests)

#include "CatalogPerformanceGuardTests.moc"
