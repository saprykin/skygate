#include "skygate/ephemeris/ConstellationDataCodec.hpp"

#include <QtTest/QtTest>

#include <string>
#include <vector>

class ConstellationDataCodecTests final : public QObject {
    Q_OBJECT

private slots:
    void lineRowsRoundTripAndSkipMalformedRows();
    void labelRowsRoundTripSanitizeAndSkipMalformedRows();
};

void ConstellationDataCodecTests::lineRowsRoundTripAndSkipMalformedRows()
{
    const std::vector<skygate::ephemeris::ConstellationLineRef> lineRefs {
        {"hip_1", "hip_2"},
        {"", "hip_3"},
        {"hip_4", "hip_5"},
    };

    const std::string rows = skygate::ephemeris::ConstellationDataCodec::serializeLineRows(
        lineRefs
    );
    QVERIFY(rows == "hip_1|hip_2\nhip_4|hip_5\n");

    const auto parsed = skygate::ephemeris::ConstellationDataCodec::parseLineRows(
        "bad\nhip_1|hip_2\n|missing\nmissing|\nhip_4|hip_5"
    );
    QCOMPARE(parsed.size(), 2U);
    QVERIFY(parsed[0].first == "hip_1");
    QVERIFY(parsed[0].second == "hip_2");
    QVERIFY(parsed[1].first == "hip_4");
    QVERIFY(parsed[1].second == "hip_5");
}

void ConstellationDataCodecTests::labelRowsRoundTripSanitizeAndSkipMalformedRows()
{
    const std::vector<skygate::ephemeris::ConstellationLabelRef> labelRefs {
        {"  Canis|Major\n ", {"hip_1", "", "hip_2"}},
        {"", {"hip_3"}},
        {"Orion", {}},
    };

    const std::string rows = skygate::ephemeris::ConstellationDataCodec::serializeLabelRows(
        labelRefs
    );
    QVERIFY(rows == "Canis/Major|hip_1,hip_2\n");

    const auto parsed = skygate::ephemeris::ConstellationDataCodec::parseLabelRows(
        "bad\nCanis/Major|hip_1,,hip_2\nMissing|\n|hip_3"
    );
    QCOMPARE(parsed.size(), 1U);
    QVERIFY(parsed[0].first == "Canis/Major");
    QCOMPARE(parsed[0].second.size(), 2U);
    QVERIFY(parsed[0].second[0] == "hip_1");
    QVERIFY(parsed[0].second[1] == "hip_2");
}

QTEST_APPLESS_MAIN(ConstellationDataCodecTests)

#include "ConstellationDataCodecTests.moc"
