#include "skygate/ephemeris/CatalogPayloadParser.hpp"

#include <QtTest/QtTest>

#include <algorithm>
#include <array>
#include <string>

namespace {

int severityRank(const QtMsgType type) noexcept
{
    switch (type) {
    case QtDebugMsg:
        return 0;
    case QtInfoMsg:
        return 1;
    case QtWarningMsg:
        return 2;
    case QtCriticalMsg:
        return 3;
    case QtFatalMsg:
        return 4;
    }

    return 1;
}

class LogCapture final {
public:
    explicit LogCapture(const QtMsgType minimumType = QtWarningMsg)
        : m_minimumType(minimumType)
    {
        s_current = this;
        m_previousHandler = qInstallMessageHandler(&LogCapture::handler);
    }

    ~LogCapture()
    {
        qInstallMessageHandler(m_previousHandler);
        s_current = nullptr;
    }

    [[nodiscard]] QString joinedMessages() const
    {
        return m_messages.join('\n');
    }

private:
    static void handler(
        const QtMsgType type,
        const QMessageLogContext& context,
        const QString& message
    )
    {
        if (
            s_current != nullptr
            && severityRank(type) >= severityRank(s_current->m_minimumType)
        ) {
            s_current->m_messages.push_back(QStringLiteral("%1 %2").arg(
                QString::fromUtf8(context.category != nullptr ? context.category : "default"),
                message
            ));
        }
    }

private:
    QtMessageHandler m_previousHandler = nullptr;
    QtMsgType m_minimumType = QtWarningMsg;
    QStringList m_messages;
    static inline LogCapture* s_current = nullptr;
};

}  // namespace

class CatalogPayloadParserTests final : public QObject {
    Q_OBJECT

private slots:
    void detectsPayloadFormats();
    void rejectsPipeRowsPayload();
    void parsesOpenNgcPayload();
    void parsesHygGzipPayload();
    void parsesHygZipPayload();
    void rejectsInvalidArchivePayloads();
    void reportsUnsupportedPayload();
    void logsSampledInvalidHygRows();
    void logsSampledInvalidOpenNgcRows();
    void logsSuccessfulParseSummariesAtInfoLevel();
    void validPayloadsStayQuietAtWarningLevel();
    void logsUnsupportedAndArchiveFailures();
};

void CatalogPayloadParserTests::detectsPayloadFormats()
{
    const skygate::ephemeris::CatalogPayloadParser parser;

    QVERIFY(
        parser.detectFormat("alpha|Alpha|Star|1.0\n")
        == skygate::ephemeris::CatalogPayloadFormat::Unknown
    );
    QVERIFY(
        parser.detectFormat("id,hip,proper,ra,dec,mag\n")
        == skygate::ephemeris::CatalogPayloadFormat::HygCsv
    );
    QVERIFY(
        parser.detectFormat("Name;Type;RA;Dec;M;NGC;IC\n")
        == skygate::ephemeris::CatalogPayloadFormat::OpenNgcCsv
    );

    constexpr std::array<unsigned char, 2> kGzipPrefix {{0x1f, 0x8b}};
    const std::string_view gzipPrefix(
        reinterpret_cast<const char*>(kGzipPrefix.data()),
        kGzipPrefix.size()
    );
    QVERIFY(
        parser.detectFormat(gzipPrefix)
        == skygate::ephemeris::CatalogPayloadFormat::HygCsvGzip
    );

    constexpr std::array<unsigned char, 4> kZipPrefix {{0x50, 0x4b, 0x03, 0x04}};
    const std::string_view zipPrefix(
        reinterpret_cast<const char*>(kZipPrefix.data()),
        kZipPrefix.size()
    );
    QVERIFY(
        parser.detectFormat(zipPrefix)
        == skygate::ephemeris::CatalogPayloadFormat::HygCsvZip
    );

    QVERIFY(
        parser.detectFormat("just some plain text")
        == skygate::ephemeris::CatalogPayloadFormat::Unknown
    );
}

void CatalogPayloadParserTests::rejectsPipeRowsPayload()
{
    const skygate::ephemeris::CatalogPayloadParser parser;
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog payload parse failed: Catalog payload format is not recognized."
    );
    const auto parseResult = parser.parseResult(
        "demo_star|Demo Star|Star|1.0|12.5|-30.0\n"
        "demo_constellation|Demo Constellation|Constellation|2.0\n"
    );
    QVERIFY(!parseResult.isSuccess());
    QVERIFY(
        parseResult.errorCode == skygate::ephemeris::CatalogLoadErrorCode::UnsupportedFormat
    );
}

void CatalogPayloadParserTests::parsesOpenNgcPayload()
{
    constexpr std::string_view kOpenNgcCsv =
        "Name;Type;RA;Dec;Const;MajAx;MinAx;PosAng;B-Mag;V-Mag;J-Mag;H-Mag;K-Mag;SurfBr;Hubble;Cstar U-Mag;Cstar B-Mag;Cstar V-Mag;M;NGC;IC;Cstar Names;Identifiers;Common names;NED notes;OpenNGC notes\n"
        "SKIP0001;Star;00:00:00.00;+00:00:00.0;Psc;;;;;;;;;;;;;;;;;;;;;;;;;;;\n"
        "NGC0224;G;00:42:44.35;+41:16:08.6;And;177.83;69.66;35;4.36;3.44;;;;13.35;SA(s)b;;;;31;0224;;;PGC 2557,UGC 454;Andromeda Galaxy;;\n"
        "NGC6720;PN;18:53:35.10;+33:01:45.0;Lyr;1.27;;0;;8.80;;;;;;;;;57;6720;;;PK 063+13 1;Ring Nebula;;\n";

    const skygate::ephemeris::CatalogPayloadParser parser;
    const auto parseResult = parser.parseResult(kOpenNgcCsv);
    QVERIFY(parseResult.isSuccess());
    QVERIFY(parseResult.catalog != nullptr);
    QVERIFY(
        parseResult.detectedFormat == skygate::ephemeris::CatalogPayloadFormat::OpenNgcCsv
    );

    const auto bodies = parseResult.catalog->bodies();
    QCOMPARE(bodies.size(), 2U);

    const auto m31It = std::find_if(
        bodies.begin(),
        bodies.end(),
        [](const skygate::ephemeris::CelestialBody& body) {
            return body.id == "messier_031";
        }
    );
    QVERIFY(m31It != bodies.end());
    QCOMPARE(m31It->displayName, std::string("M31"));
    QVERIFY(m31It->type == skygate::ephemeris::CelestialBodyType::DeepSkyObject);
    QVERIFY(
        m31It->ephemerisSource
        == skygate::ephemeris::CelestialBodyEphemerisSource::FixedEquatorial
    );
    QVERIFY(m31It->deepSkyObject.has_value());
    QVERIFY(m31It->deepSkyObject->kind == skygate::ephemeris::DeepSkyObjectKind::Galaxy);
    QVERIFY(m31It->deepSkyObject->majorAxisArcmin.has_value());
    QCOMPARE(*m31It->deepSkyObject->majorAxisArcmin, 177.83);
    QVERIFY(
        std::find(
            m31It->deepSkyObject->aliases.begin(),
            m31It->deepSkyObject->aliases.end(),
            std::string("Andromeda Galaxy")
        ) != m31It->deepSkyObject->aliases.end()
    );

    const auto m57It = std::find_if(
        bodies.begin(),
        bodies.end(),
        [](const skygate::ephemeris::CelestialBody& body) {
            return body.id == "messier_057";
        }
    );
    QVERIFY(m57It != bodies.end());
    QVERIFY(
        m57It->deepSkyObject.has_value()
        && m57It->deepSkyObject->kind
            == skygate::ephemeris::DeepSkyObjectKind::PlanetaryNebula
    );
}

void CatalogPayloadParserTests::parsesHygGzipPayload()
{
    constexpr std::array<unsigned char, 73> kCompressedCsv {
        0x1f, 0x8b, 0x08, 0x00, 0x77, 0xa9, 0x86, 0x69, 0x00, 0x03, 0xcb, 0x4c,
        0xd1, 0xc9, 0xc8, 0x2c, 0xd0, 0x29, 0x28, 0xca, 0x2f, 0x48, 0x2d, 0xd2,
        0x29, 0x4a, 0xd4, 0x49, 0x49, 0x4d, 0xd6, 0xc9, 0x4d, 0x4c, 0xe7, 0x32,
        0xd7, 0x31, 0x31, 0xd2, 0x09, 0x49, 0x2d, 0x2e, 0x09, 0x2e, 0x49, 0x2c,
        0xd2, 0x31, 0xd4, 0x33, 0x32, 0xd5, 0xd1, 0x35, 0xd2, 0x33, 0xd5, 0x31,
        0xd6, 0x33, 0xe2, 0x02, 0x00, 0xb9, 0xd5, 0xee, 0x71, 0x35, 0x00, 0x00,
        0x00
    };
    const std::string_view compressedData(
        reinterpret_cast<const char*>(kCompressedCsv.data()),
        kCompressedCsv.size()
    );

    const skygate::ephemeris::CatalogPayloadParser parser;
    const auto catalog = parser.parse(compressedData);
    QVERIFY(catalog != nullptr);

    const auto bodies = catalog->bodies();
    QVERIFY(bodies.size() == 1U);
    QVERIFY(bodies[0].id == "hip_42");
}

void CatalogPayloadParserTests::parsesHygZipPayload()
{
    constexpr std::array<unsigned char, 213> kZippedCsv {
        0x50, 0x4b, 0x03, 0x04, 0x14, 0x00, 0x00, 0x00, 0x08, 0x00, 0xc8, 0xa0,
        0x5c, 0x5c, 0x76, 0xd9, 0x48, 0x5a, 0x31, 0x00, 0x00, 0x00, 0x32, 0x00,
        0x00, 0x00, 0x07, 0x00, 0x1c, 0x00, 0x68, 0x79, 0x67, 0x2e, 0x63, 0x73,
        0x76, 0x55, 0x54, 0x09, 0x00, 0x03, 0xa8, 0x3c, 0xa3, 0x69, 0xa8, 0x3c,
        0xa3, 0x69, 0x75, 0x78, 0x0b, 0x00, 0x01, 0x04, 0xf5, 0x01, 0x00, 0x00,
        0x04, 0x14, 0x00, 0x00, 0x00, 0xcb, 0x4c, 0xd1, 0xc9, 0xc8, 0x2c, 0xd0,
        0x29, 0x28, 0xca, 0x2f, 0x48, 0x2d, 0xd2, 0x29, 0x4a, 0xd4, 0x49, 0x49,
        0x4d, 0xd6, 0xc9, 0x4d, 0x4c, 0xe7, 0x32, 0x31, 0xd2, 0x01, 0x22, 0x97,
        0xd4, 0xdc, 0x7c, 0x1d, 0x43, 0x3d, 0x23, 0x53, 0x1d, 0x5d, 0x23, 0x3d,
        0x53, 0x20, 0xcb, 0x80, 0x0b, 0x00, 0x50, 0x4b, 0x01, 0x02, 0x1e, 0x03,
        0x14, 0x00, 0x00, 0x00, 0x08, 0x00, 0xc8, 0xa0, 0x5c, 0x5c, 0x76, 0xd9,
        0x48, 0x5a, 0x31, 0x00, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0x07, 0x00,
        0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0xa4, 0x81,
        0x00, 0x00, 0x00, 0x00, 0x68, 0x79, 0x67, 0x2e, 0x63, 0x73, 0x76, 0x55,
        0x54, 0x05, 0x00, 0x03, 0xa8, 0x3c, 0xa3, 0x69, 0x75, 0x78, 0x0b, 0x00,
        0x01, 0x04, 0xf5, 0x01, 0x00, 0x00, 0x04, 0x14, 0x00, 0x00, 0x00, 0x50,
        0x4b, 0x05, 0x06, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x4d,
        0x00, 0x00, 0x00, 0x72, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    const std::string_view zippedData(
        reinterpret_cast<const char*>(kZippedCsv.data()),
        kZippedCsv.size()
    );

    const skygate::ephemeris::CatalogPayloadParser parser;
    const auto catalog = parser.parse(zippedData);
    QVERIFY(catalog != nullptr);

    const auto bodies = catalog->bodies();
    QVERIFY(bodies.size() == 1U);
    QVERIFY(bodies[0].id == "hip_42");
}

void CatalogPayloadParserTests::rejectsInvalidArchivePayloads()
{
    const skygate::ephemeris::CatalogPayloadParser parser;

    const std::array<unsigned char, 4> invalidGzip {{0x1f, 0x8b, 0x00, 0x00}};
    const std::string_view invalidGzipData(
        reinterpret_cast<const char*>(invalidGzip.data()),
        invalidGzip.size()
    );
    QTest::ignoreMessage(
        QtWarningMsg,
        "Gzip catalog parse failed: Gzip catalog payload could not be decompressed."
    );
    const auto invalidGzipResult = parser.parseResult(invalidGzipData);
    QVERIFY(!invalidGzipResult.isSuccess());
    QCOMPARE(
        invalidGzipResult.errorCode,
        skygate::ephemeris::CatalogLoadErrorCode::InvalidGzipData
    );

    constexpr std::array<unsigned char, 22> kEmptyZip {
        0x50, 0x4b, 0x05, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    const std::string_view emptyZipData(
        reinterpret_cast<const char*>(kEmptyZip.data()),
        kEmptyZip.size()
    );
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog ZIP parse failed: ZIP catalog payload does not contain a readable CSV entry."
    );
    const auto emptyZipResult = parser.parseResult(emptyZipData);
    QVERIFY(!emptyZipResult.isSuccess());
    QCOMPARE(
        emptyZipResult.errorCode,
        skygate::ephemeris::CatalogLoadErrorCode::InvalidZipData
    );
}

void CatalogPayloadParserTests::reportsUnsupportedPayload()
{
    const skygate::ephemeris::CatalogPayloadParser parser;
    QTest::ignoreMessage(
        QtWarningMsg,
        "Catalog payload parse failed: Catalog payload format is not recognized."
    );
    auto parseResult = parser.parseResult("just some plain text");

    QVERIFY(!parseResult.isSuccess());
    QVERIFY(
        parseResult.errorCode == skygate::ephemeris::CatalogLoadErrorCode::UnsupportedFormat
    );
}

void CatalogPayloadParserTests::logsSampledInvalidHygRows()
{
    std::string payload = "id,hip,proper,ra,dec,mag\n";
    for (int index = 0; index < 8; ++index) {
        payload += std::to_string(index + 1) + ",,Bad Star,bad,-16.0,1.0\n";
    }
    payload += "9,42,Sirius,6.7525,-16.7161,-1.46\n";

    LogCapture capture;
    const skygate::ephemeris::CatalogPayloadParser parser;
    const auto result = parser.parseResult(payload);

    QVERIFY(result.isSuccess());
    const QString messages = capture.joinedMessages();
    QVERIFY(messages.contains(QStringLiteral("skygate.catalog.parse")));
    QVERIFY(messages.contains(QStringLiteral("HYG CSV skipped 8 rows")));
    QVERIFY(messages.contains(QStringLiteral("row 2")));
    QVERIFY(messages.contains(QStringLiteral("row 6")));
    QVERIFY(!messages.contains(QStringLiteral("row 7")));
}

void CatalogPayloadParserTests::logsSampledInvalidOpenNgcRows()
{
    constexpr std::string_view kOpenNgcCsv =
        "Name;Type;RA;Dec;Const;MajAx;MinAx;PosAng;B-Mag;V-Mag;J-Mag;H-Mag;K-Mag;SurfBr;Hubble;Cstar U-Mag;Cstar B-Mag;Cstar V-Mag;M;NGC;IC;Cstar Names;Identifiers;Common names;NED notes;OpenNGC notes\n"
        "BAD0001;G;bad;+41:16:08.6;And;;;;;;;;;;;;;;;;;;;;0224;;;;\n"
        "BAD0002;G;00:42:44.35;bad;And;;;;;;;;;;;;;;;;;;;;0225;;;;\n"
        "NGC0224;G;00:42:44.35;+41:16:08.6;And;177.83;69.66;35;4.36;3.44;;;;13.35;SA(s)b;;;;31;0224;;;PGC 2557;Andromeda Galaxy;;\n";

    LogCapture capture;
    const skygate::ephemeris::CatalogPayloadParser parser;
    const auto result = parser.parseResult(kOpenNgcCsv);

    QVERIFY(result.isSuccess());
    const QString messages = capture.joinedMessages();
    QVERIFY(messages.contains(QStringLiteral("OpenNGC CSV skipped 2 rows")));
    QVERIFY(messages.contains(QStringLiteral("invalid coordinates")));
    QVERIFY(messages.contains(QStringLiteral("row 2")));
    QVERIFY(messages.contains(QStringLiteral("row 3")));
}

void CatalogPayloadParserTests::logsSuccessfulParseSummariesAtInfoLevel()
{
    constexpr std::string_view kHygCsv =
        "id,hip,proper,ra,dec,mag\n"
        "1,42,Sirius,6.7525,-16.7161,-1.46\n";

    LogCapture capture(QtInfoMsg);
    const skygate::ephemeris::CatalogPayloadParser parser;
    const auto result = parser.parseResult(kHygCsv);

    QVERIFY(result.isSuccess());
    const QString messages = capture.joinedMessages();
    QVERIFY(messages.contains(QStringLiteral("skygate.catalog.parse")));
    QVERIFY(messages.contains(QStringLiteral("Catalog payload parsed: format HYG CSV")));
    QVERIFY(messages.contains(QStringLiteral("parsed 1")));
    QVERIFY(messages.contains(QStringLiteral("selected 1")));
    QVERIFY(!messages.contains(QStringLiteral("row 2")));
}

void CatalogPayloadParserTests::validPayloadsStayQuietAtWarningLevel()
{
    constexpr std::string_view kHygCsv =
        "id,hip,proper,ra,dec,mag\n"
        "1,42,Sirius,6.7525,-16.7161,-1.46\n";

    LogCapture capture;
    const skygate::ephemeris::CatalogPayloadParser parser;
    const auto result = parser.parseResult(kHygCsv);

    QVERIFY(result.isSuccess());
    QVERIFY(capture.joinedMessages().isEmpty());
}

void CatalogPayloadParserTests::logsUnsupportedAndArchiveFailures()
{
    LogCapture capture;
    const skygate::ephemeris::CatalogPayloadParser parser;
    (void)parser.parseResult("just some plain text");

    const std::array<unsigned char, 4> invalidGzip {{0x1f, 0x8b, 0x00, 0x00}};
    const std::string_view invalidGzipData(
        reinterpret_cast<const char*>(invalidGzip.data()),
        invalidGzip.size()
    );
    (void)parser.parseResult(invalidGzipData);

    constexpr std::array<unsigned char, 22> kEmptyZip {
        0x50, 0x4b, 0x05, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    const std::string_view emptyZipData(
        reinterpret_cast<const char*>(kEmptyZip.data()),
        kEmptyZip.size()
    );
    (void)parser.parseResult(emptyZipData);

    const QString messages = capture.joinedMessages();
    QVERIFY(messages.contains(QStringLiteral("Catalog payload format is not recognized")));
    QVERIFY(messages.contains(QStringLiteral("Gzip catalog payload could not be decompressed")));
    QVERIFY(messages.contains(QStringLiteral("ZIP catalog payload does not contain a readable CSV entry")));
}

QTEST_APPLESS_MAIN(CatalogPayloadParserTests)

#include "CatalogPayloadParserTests.moc"
