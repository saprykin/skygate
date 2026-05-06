#include "skygate/ephemeris/CatalogPayloadParser.hpp"
#include "skygate/testsupport/DeterministicFuzz.hpp"
#include "skygate/testsupport/LogCapture.hpp"

#include <QtTest/QtTest>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace {

[[nodiscard]] std::string fixedDouble(const double value, const int precision = 6)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(precision) << value;
    return stream.str();
}

[[nodiscard]] std::string joinFields(const std::vector<std::string>& fields, const char separator)
{
    std::string row;
    for (std::size_t index = 0; index < fields.size(); ++index) {
        if (index > 0U) {
            row.push_back(separator);
        }
        row += fields[index];
    }
    row.push_back('\n');
    return row;
}

[[nodiscard]] std::string twoDigit(const int value)
{
    std::ostringstream stream;
    stream << std::setw(2) << std::setfill('0') << value;
    return stream.str();
}

[[nodiscard]] std::string sexagesimalRa(skygate::testsupport::DeterministicRng& rng)
{
    return twoDigit(rng.intInRange(0, 23)) + ":"
        + twoDigit(rng.intInRange(0, 59)) + ":"
        + twoDigit(rng.intInRange(0, 59)) + ".00";
}

[[nodiscard]] std::string sexagesimalDec(skygate::testsupport::DeterministicRng& rng)
{
    const int sign = rng.chance(1U, 2U) ? 1 : -1;
    const int degrees = rng.intInRange(0, 89);
    return std::string(sign >= 0 ? "+" : "-")
        + twoDigit(degrees) + ":"
        + twoDigit(rng.intInRange(0, 59)) + ":"
        + twoDigit(rng.intInRange(0, 59)) + ".0";
}

[[nodiscard]] bool hasFiniteCatalogBodies(const skygate::ephemeris::CatalogLoadResult& result)
{
    if (result.catalog == nullptr) {
        return false;
    }

    for (const skygate::ephemeris::CelestialBody& body : result.catalog->bodies()) {
        if (body.id.empty() || body.displayName.empty()) {
            return false;
        }
        if (!std::isfinite(body.visualMagnitude)) {
            return false;
        }
        if (!body.fixedEquatorial.has_value()) {
            return false;
        }
        if (
            !std::isfinite(body.fixedEquatorial->rightAscensionHours)
            || !std::isfinite(body.fixedEquatorial->declinationDeg)
        ) {
            return false;
        }
    }
    return true;
}

}  // namespace

class CatalogPropertyTests final : public QObject {
    Q_OBJECT

private slots:
    void randomUnknownPayloadsReturnExplicitFailures();
    void generatedHygPayloadsKeepExactlyTheFiniteRows();
    void generatedOpenNgcPayloadsKeepExactlyTheValidMappedRows();
};

void CatalogPropertyTests::randomUnknownPayloadsReturnExplicitFailures()
{
    const skygate::ephemeris::CatalogPayloadParser parser;
    skygate::testsupport::DeterministicRng rng(0x5eedcafeU);
    skygate::testsupport::LogCapture capture;

    for (int sample = 0; sample < 160; ++sample) {
        const std::size_t length = static_cast<std::size_t>(rng.intInRange(0, 96));
        const std::string payload = rng.token(
            length,
            "xyzXYZ0123456789 \t|:/{}[]"
        );

        const auto result = parser.parseResult(payload);
        QVERIFY(!result.isSuccess());
        QVERIFY(result.catalog == nullptr);
        QVERIFY(result.errorCode != skygate::ephemeris::CatalogLoadErrorCode::None);
        QVERIFY(!result.errorDetail.empty());
        if (payload.empty()) {
            QCOMPARE(result.errorCode, skygate::ephemeris::CatalogLoadErrorCode::EmptyInput);
        } else {
            QCOMPARE(result.detectedFormat, skygate::ephemeris::CatalogPayloadFormat::Unknown);
            QCOMPARE(
                result.errorCode,
                skygate::ephemeris::CatalogLoadErrorCode::UnsupportedFormat
            );
        }
    }

    QVERIFY(capture.joinedMessages().contains(QStringLiteral("Catalog payload parse failed")));
}

void CatalogPropertyTests::generatedHygPayloadsKeepExactlyTheFiniteRows()
{
    const skygate::ephemeris::CatalogPayloadParser parser;

    for (int scenario = 0; scenario < 32; ++scenario) {
        skygate::testsupport::DeterministicRng rng(0x48494700U + scenario);
        std::string payload = "proper,mag,dec,id,hip,ra,bf\n";
        std::size_t validRows = 0U;
        std::size_t invalidRows = 0U;
        const int rowCount = 36 + scenario;

        for (int row = 0; row < rowCount; ++row) {
            const bool valid = (row % 5) != (scenario % 5);
            if (valid) {
                ++validRows;
                payload += joinFields(
                    {
                        "Generated Star " + std::to_string(scenario) + "-" + std::to_string(row),
                        fixedDouble(rng.realInRange(-1.5, 12.0), 3),
                        fixedDouble(rng.realInRange(-89.0, 89.0), 6),
                        std::to_string((scenario + 1) * 1000 + row),
                        std::to_string((scenario + 1) * 10000 + row),
                        fixedDouble(rng.realInRange(0.0, 23.999), 6),
                        "Alp Test"
                    },
                    ','
                );
            } else {
                ++invalidRows;
                const int invalidField = rng.intInRange(0, 2);
                payload += joinFields(
                    {
                        "Broken Star " + std::to_string(row),
                        invalidField == 0 ? "not-mag" : fixedDouble(rng.realInRange(-1.5, 12.0)),
                        invalidField == 1 ? "bad-dec" : fixedDouble(rng.realInRange(-89.0, 89.0)),
                        std::to_string(row),
                        "",
                        invalidField == 2 ? "bad-ra" : fixedDouble(rng.realInRange(0.0, 23.999)),
                        ""
                    },
                    ','
                );
            }
        }

        skygate::testsupport::LogCapture capture;
        const auto result = parser.parseResult(payload);

        QVERIFY(result.isSuccess());
        QCOMPARE(result.detectedFormat, skygate::ephemeris::CatalogPayloadFormat::HygCsv);
        QCOMPARE(result.diagnostics.processedRowCount, static_cast<std::size_t>(rowCount));
        QCOMPARE(result.diagnostics.parsedBodyCount, validRows);
        QCOMPARE(result.diagnostics.selectedBodyCount, validRows);
        QCOMPARE(result.diagnostics.truncatedBodyCount, std::size_t(0));
        QCOMPARE(result.catalog->bodies().size(), validRows);
        QVERIFY(hasFiniteCatalogBodies(result));
        if (invalidRows > 0U) {
            QVERIFY(capture.joinedMessages().contains(QStringLiteral("HYG CSV skipped")));
        }
    }
}

void CatalogPropertyTests::generatedOpenNgcPayloadsKeepExactlyTheValidMappedRows()
{
    const skygate::ephemeris::CatalogPayloadParser parser;
    constexpr const char* kHeader =
        "Name;Type;RA;Dec;Const;MajAx;MinAx;PosAng;B-Mag;V-Mag;J-Mag;H-Mag;K-Mag;SurfBr;Hubble;Cstar U-Mag;Cstar B-Mag;Cstar V-Mag;M;NGC;IC;Cstar Names;Identifiers;Common names;NED notes;OpenNGC notes\n";
    const std::vector<std::string> supportedTypes {"G", "PN", "OC", "Gb", "N"};

    for (int scenario = 0; scenario < 28; ++scenario) {
        skygate::testsupport::DeterministicRng rng(0x0e46c000U + scenario);
        std::string payload = kHeader;
        std::size_t validRows = 0U;
        std::size_t invalidRows = 0U;
        const int rowCount = 30 + scenario;

        for (int row = 0; row < rowCount; ++row) {
            const bool valid = (row % 6) != (scenario % 6);
            std::vector<std::string> fields(26);
            const int catalogNumber = 1000 + (scenario * 100) + row;
            fields[0] = "NGC" + std::to_string(catalogNumber);
            fields[1] = supportedTypes.at(rng.index(supportedTypes.size()));
            fields[2] = valid ? sexagesimalRa(rng) : "99:99:99.0";
            fields[3] = valid ? sexagesimalDec(rng) : "+91:00:00.0";
            fields[4] = "Ori";
            fields[5] = fixedDouble(rng.realInRange(0.1, 120.0), 2);
            fields[6] = fixedDouble(rng.realInRange(0.1, 90.0), 2);
            fields[7] = std::to_string(rng.intInRange(0, 179));
            fields[9] = fixedDouble(rng.realInRange(3.0, 14.0), 2);
            fields[19] = std::to_string(catalogNumber);
            fields[22] = "PGC " + std::to_string(catalogNumber);
            fields[23] = "Generated Object " + std::to_string(row);
            payload += joinFields(fields, ';');

            valid ? ++validRows : ++invalidRows;
        }

        skygate::testsupport::LogCapture capture;
        const auto result = parser.parseResult(payload);

        QVERIFY(result.isSuccess());
        QCOMPARE(result.detectedFormat, skygate::ephemeris::CatalogPayloadFormat::OpenNgcCsv);
        QCOMPARE(result.diagnostics.processedRowCount, static_cast<std::size_t>(rowCount));
        QCOMPARE(result.diagnostics.parsedBodyCount, validRows);
        QCOMPARE(result.diagnostics.selectedBodyCount, validRows);
        QCOMPARE(result.diagnostics.truncatedBodyCount, std::size_t(0));
        QCOMPARE(result.catalog->bodies().size(), validRows);
        QVERIFY(hasFiniteCatalogBodies(result));
        if (invalidRows > 0U) {
            QVERIFY(capture.joinedMessages().contains(QStringLiteral("OpenNGC CSV skipped")));
        }
    }
}

QTEST_APPLESS_MAIN(CatalogPropertyTests)

#include "CatalogPropertyTests.moc"
