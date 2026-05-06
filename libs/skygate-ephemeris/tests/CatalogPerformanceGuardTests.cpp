#include "skygate/ephemeris/CatalogPayloadParser.hpp"
#include "catalog/ZipCodec.hpp"

#include <QtTest/QtTest>

#include <QElapsedTimer>

#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

class CatalogPerformanceGuardTests final : public QObject {
    Q_OBJECT

private slots:
    void parsesModeratelyLargeHygCatalogWithinGuardrail();
    void parsesModeratelyLargeOpenNgcCatalogWithinGuardrail();
    void scansZipWithManyEntriesWithinGuardrail();
};

namespace {

constexpr qint64 kLargeCatalogParseBudgetMs = 15000;
constexpr qint64 kZipScanBudgetMs = 5000;

bool isStrictPerformanceGuardMode()
{
    const QString mode = QString::fromUtf8(
        qgetenv("SKYGATE_PERFORMANCE_GUARD_MODE")
    ).trimmed().toLower();
    const QString legacyStrict = QString::fromUtf8(
        qgetenv("SKYGATE_STRICT_PERFORMANCE_GUARDS")
    ).trimmed().toLower();
    return mode == QStringLiteral("strict")
        || (
            !legacyStrict.isEmpty()
            && legacyStrict != QStringLiteral("0")
            && legacyStrict != QStringLiteral("false")
            && legacyStrict != QStringLiteral("no")
            && legacyStrict != QStringLiteral("off")
        );
}

QString performanceMetricMessage(
    const qint64 elapsedMs,
    const qint64 budgetMs,
    const char* operationName,
    const bool strictMode
)
{
    const qint64 deltaMs = budgetMs - elapsedMs;
    const double budgetPercent = budgetMs > 0
        ? (static_cast<double>(elapsedMs) * 100.0 / static_cast<double>(budgetMs))
        : 0.0;
    return QStringLiteral(
        "perf %1: %2 elapsed=%3 ms budget=%4 ms delta=%5 ms budget_used=%6% "
        "strict=%7"
    )
        .arg(elapsedMs < budgetMs ? QStringLiteral("within-budget")
                                  : QStringLiteral("over-budget"))
        .arg(QString::fromUtf8(operationName))
        .arg(elapsedMs)
        .arg(budgetMs)
        .arg(deltaMs)
        .arg(QString::number(budgetPercent, 'f', 1))
        .arg(strictMode ? QStringLiteral("on") : QStringLiteral("off"));
}

struct ZipEntrySpec final {
    std::string path;
    std::string data;
};

void appendLe16(std::string& data, const std::uint16_t value)
{
    data.push_back(static_cast<char>(value & 0xffU));
    data.push_back(static_cast<char>((value >> 8U) & 0xffU));
}

void appendLe32(std::string& data, const std::uint32_t value)
{
    appendLe16(data, static_cast<std::uint16_t>(value & 0xffffU));
    appendLe16(data, static_cast<std::uint16_t>((value >> 16U) & 0xffffU));
}

std::string makeZip(const std::vector<ZipEntrySpec>& entries)
{
    std::string zipData;
    std::vector<std::uint32_t> localHeaderOffsets;
    localHeaderOffsets.reserve(entries.size());

    for (const ZipEntrySpec& entry : entries) {
        localHeaderOffsets.push_back(static_cast<std::uint32_t>(zipData.size()));
        appendLe32(zipData, 0x04034b50U);
        appendLe16(zipData, 20U);
        appendLe16(zipData, 0U);
        appendLe16(zipData, 0U);
        appendLe16(zipData, 0U);
        appendLe16(zipData, 0U);
        appendLe32(zipData, 0U);
        appendLe32(zipData, static_cast<std::uint32_t>(entry.data.size()));
        appendLe32(zipData, static_cast<std::uint32_t>(entry.data.size()));
        appendLe16(zipData, static_cast<std::uint16_t>(entry.path.size()));
        appendLe16(zipData, 0U);
        zipData += entry.path;
        zipData += entry.data;
    }

    const std::uint32_t centralDirectoryOffset = static_cast<std::uint32_t>(zipData.size());
    for (std::size_t index = 0; index < entries.size(); ++index) {
        const ZipEntrySpec& entry = entries[index];
        appendLe32(zipData, 0x02014b50U);
        appendLe16(zipData, 20U);
        appendLe16(zipData, 20U);
        appendLe16(zipData, 0U);
        appendLe16(zipData, 0U);
        appendLe16(zipData, 0U);
        appendLe16(zipData, 0U);
        appendLe32(zipData, 0U);
        appendLe32(zipData, static_cast<std::uint32_t>(entry.data.size()));
        appendLe32(zipData, static_cast<std::uint32_t>(entry.data.size()));
        appendLe16(zipData, static_cast<std::uint16_t>(entry.path.size()));
        appendLe16(zipData, 0U);
        appendLe16(zipData, 0U);
        appendLe16(zipData, 0U);
        appendLe16(zipData, 0U);
        appendLe32(zipData, 0U);
        appendLe32(zipData, localHeaderOffsets[index]);
        zipData += entry.path;
    }

    const std::uint32_t centralDirectorySize =
        static_cast<std::uint32_t>(zipData.size()) - centralDirectoryOffset;
    appendLe32(zipData, 0x06054b50U);
    appendLe16(zipData, 0U);
    appendLe16(zipData, 0U);
    appendLe16(zipData, static_cast<std::uint16_t>(entries.size()));
    appendLe16(zipData, static_cast<std::uint16_t>(entries.size()));
    appendLe32(zipData, centralDirectorySize);
    appendLe32(zipData, centralDirectoryOffset);
    appendLe16(zipData, 0U);
    return zipData;
}

void verifyElapsedBelow(
    const qint64 elapsedMs,
    const qint64 budgetMs,
    const char* operationName
)
{
    const bool strictMode = isStrictPerformanceGuardMode();
    const QString message = performanceMetricMessage(
        elapsedMs,
        budgetMs,
        operationName,
        strictMode
    );

    if (elapsedMs < budgetMs) {
        qInfo().noquote() << message;
        return;
    }

    const QString strictHint = QStringLiteral(
        "%1; set SKYGATE_PERFORMANCE_GUARD_MODE=strict or "
        "SKYGATE_STRICT_PERFORMANCE_GUARDS=1 to make advisory budgets fail"
    ).arg(message);
    if (!strictMode) {
        qWarning().noquote() << strictHint;
        return;
    }

    QVERIFY2(false, qPrintable(strictHint));
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

void CatalogPerformanceGuardTests::scansZipWithManyEntriesWithinGuardrail()
{
    std::vector<ZipEntrySpec> entries;
    entries.reserve(3001U);
    for (int index = 0; index < 3000; ++index) {
        entries.push_back(ZipEntrySpec {
            .path = "notes/readme-" + std::to_string(index) + ".txt",
            .data = "not a catalog\n"
        });
    }
    entries.push_back(ZipEntrySpec {
        .path = "catalogs/hyg.csv",
        .data = "id,hip,proper,ra,dec,mag\n1,42,Target,1.0,2.0,3.0\n"
    });
    const std::string zipData = makeZip(entries);

    QElapsedTimer timer;
    timer.start();
    const auto extracted = skygate::ephemeris::ZipCodec {}.extractFirstCsvEntry(zipData);
    const qint64 elapsedMs = timer.elapsed();

    QVERIFY(extracted.has_value());
    QVERIFY(extracted->find("Target") != std::string::npos);
    verifyElapsedBelow(elapsedMs, kZipScanBudgetMs, "large ZIP entry scan");
}

QTEST_APPLESS_MAIN(CatalogPerformanceGuardTests)

#include "CatalogPerformanceGuardTests.moc"
