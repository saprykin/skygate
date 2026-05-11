#include "catalog/stellarium/StellariumHipParser.hpp"
#include "catalog/stellarium/StellariumLabelRefExtractor.hpp"
#include "catalog/stellarium/StellariumLineRefExtractor.hpp"

#include <QJsonArray>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QtTest/QtTest>

#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace {

QJsonObject makeStellariumRoot()
{
    constexpr std::string_view kJsonPayload = R"({
        "constellations": {
            "orion": {
                "common_name": { "english": "Orion" },
                "lines": [[{"hip": 24436}, "hip_25930", 26727], [26727, 24436]]
            },
            "ursa-major": {
                "line": [["hip 1", "hip_2"]]
            }
        }
    })";
    const QJsonDocument document = QJsonDocument::fromJson(QByteArray(
        kJsonPayload.data(),
        static_cast<qsizetype>(kJsonPayload.size())
    ));
    return document.object();
}

}  // namespace

class StellariumExtractorTests final : public QObject {
    Q_OBJECT

private slots:
    void extractsLineRefs();
    void extractsLabelRefs();
    void dropsMalformedAndDuplicateLineRefs();
    void mergesDuplicateLabelsAndUsesFallbackNames();
    void prefersCommonNameFieldsInOrder();
    void supportsArrayFormConstellations();
    void parsesStrictHipText();
    void rejectsMalformedHipText();
    void parsesHipIdentifiersFromJsonValues();
    void rejectsInvalidHipPolylinesAndCollectsNestedObjects();
};

void StellariumExtractorTests::extractsLineRefs()
{
    const auto lineRefs = skygate::ephemeris::StellariumLineRefExtractor::extract(
        makeStellariumRoot()
    );

    QVERIFY(lineRefs.size() == 4U);
}

void StellariumExtractorTests::extractsLabelRefs()
{
    const auto labelRefs = skygate::ephemeris::StellariumLabelRefExtractor::extract(
        makeStellariumRoot()
    );

    QVERIFY(labelRefs.size() == 2U);
    QCOMPARE(labelRefs[0].first, std::string("Orion"));
}

void StellariumExtractorTests::dropsMalformedAndDuplicateLineRefs()
{
    constexpr std::string_view kJsonPayload = R"({
        "constellations": {
            "demo": {
                "lines": [
                    [1, 2, 3],
                    [3, 2],
                    [4],
                    [5, "not_hip", 6],
                    [7, 7]
                ]
            }
        }
    })";
    const QJsonDocument document = QJsonDocument::fromJson(QByteArray(
        kJsonPayload.data(),
        static_cast<qsizetype>(kJsonPayload.size())
    ));

    const auto lineRefs = skygate::ephemeris::StellariumLineRefExtractor::extract(
        document.object()
    );

    QCOMPARE(lineRefs.size(), 2U);
    QCOMPARE(lineRefs[0].first, std::string("hip_1"));
    QCOMPARE(lineRefs[0].second, std::string("hip_2"));
    QCOMPARE(lineRefs[1].first, std::string("hip_2"));
    QCOMPARE(lineRefs[1].second, std::string("hip_3"));
}

void StellariumExtractorTests::mergesDuplicateLabelsAndUsesFallbackNames()
{
    constexpr std::string_view kJsonPayload = R"({
        "constellations": {
            "ursa-major": {
                "name": "Ursa Major",
                "lines": [[1, 2]]
            },
            "ursa_major": {
                "label": "  ursa major  ",
                "lines": [[2, 3]]
            },
            "canis-minor": {
                "lines": [[4, 5]]
            }
        }
    })";
    const QJsonDocument document = QJsonDocument::fromJson(QByteArray(
        kJsonPayload.data(),
        static_cast<qsizetype>(kJsonPayload.size())
    ));

    const auto labelRefs = skygate::ephemeris::StellariumLabelRefExtractor::extract(
        document.object()
    );

    QCOMPARE(labelRefs.size(), 2U);
    QCOMPARE(labelRefs[0].first, std::string("Canis Minor"));
    QCOMPARE(labelRefs[0].second.size(), 2U);
    QCOMPARE(labelRefs[1].first, std::string("Ursa Major"));
    QCOMPARE(labelRefs[1].second.size(), 3U);
    QCOMPARE(labelRefs[1].second[0], std::string("hip_1"));
    QCOMPARE(labelRefs[1].second[1], std::string("hip_2"));
    QCOMPARE(labelRefs[1].second[2], std::string("hip_3"));
}

void StellariumExtractorTests::prefersCommonNameFieldsInOrder()
{
    constexpr std::string_view kJsonPayload = R"({
        "constellations": {
            "demo": {
                "common_name": {
                    "native": "Native Demo",
                    "iau": "IAU Demo"
                },
                "lines": [[1, 2]]
            }
        }
    })";
    const QJsonDocument document = QJsonDocument::fromJson(QByteArray(
        kJsonPayload.data(),
        static_cast<qsizetype>(kJsonPayload.size())
    ));

    const auto labelRefs = skygate::ephemeris::StellariumLabelRefExtractor::extract(
        document.object()
    );

    QCOMPARE(labelRefs.size(), 1U);
    QCOMPARE(labelRefs[0].first, std::string("Native Demo"));
}

void StellariumExtractorTests::supportsArrayFormConstellations()
{
    constexpr std::string_view kJsonPayload = R"({
        "constellations": [
            {
                "english": "Array Demo",
                "lines": [[1, 2, 3]]
            },
            [[4, 5]]
        ]
    })";
    const QJsonDocument document = QJsonDocument::fromJson(QByteArray(
        kJsonPayload.data(),
        static_cast<qsizetype>(kJsonPayload.size())
    ));

    const auto lineRefs = skygate::ephemeris::StellariumLineRefExtractor::extract(
        document.object()
    );
    const auto labelRefs = skygate::ephemeris::StellariumLabelRefExtractor::extract(
        document.object()
    );

    QCOMPARE(lineRefs.size(), 3U);
    QCOMPARE(labelRefs.size(), 1U);
    QCOMPARE(labelRefs[0].first, std::string("Array Demo"));
}

void StellariumExtractorTests::parsesStrictHipText()
{
    const auto hip = skygate::ephemeris::StellariumHipParser::parseStrictHipText(" hip __ 24436 ");

    QVERIFY(hip.has_value());
    QCOMPARE(*hip, 24436);
}

void StellariumExtractorTests::rejectsMalformedHipText()
{
    const std::vector<QString> malformedValues {
        "",
        "   ",
        "hip",
        "hip_",
        "hip_0",
        "hip_-1",
        "hip_12a",
        "not_hip",
        QString::number(std::numeric_limits<qint64>::max())
    };

    for (const QString& value : malformedValues) {
        QVERIFY(!skygate::ephemeris::StellariumHipParser::parseStrictHipText(value).has_value());
    }
}

void StellariumExtractorTests::parsesHipIdentifiersFromJsonValues()
{
    using skygate::ephemeris::StellariumHipParser;

    QCOMPARE(*StellariumHipParser::parseHipIdentifier(QJsonValue(123)), 123);
    QCOMPARE(*StellariumHipParser::parseHipIdentifier(QJsonValue("hip 456")), 456);
    QCOMPARE(*StellariumHipParser::parseHipIdentifier(QJsonObject {{"HIP", 789}}), 789);
    QCOMPARE(*StellariumHipParser::parseHipIdentifier(QJsonObject {{"id", "hip_2468"}}), 2468);
    QCOMPARE(*StellariumHipParser::parseHipIdentifier(QJsonObject {{"star", QJsonObject {{"hip", 1357}}}}), 1357);

    QVERIFY(!StellariumHipParser::parseHipIdentifier(QJsonValue(12.5)).has_value());
    QVERIFY(!StellariumHipParser::parseHipIdentifier(QJsonValue(-1)).has_value());
    QVERIFY(!StellariumHipParser::parseHipIdentifier(QJsonValue()).has_value());
    QVERIFY(!StellariumHipParser::parseHipIdentifier(QJsonObject {{"other", 1}}).has_value());
    QVERIFY(!StellariumHipParser::parseHipIdentifier(QJsonObject {{"hip", "bad"}}).has_value());
}

void StellariumExtractorTests::rejectsInvalidHipPolylinesAndCollectsNestedObjects()
{
    using skygate::ephemeris::StellariumHipParser;

    QVERIFY(!StellariumHipParser::parseHipPolyline(QJsonArray {1}).has_value());
    QVERIFY(!StellariumHipParser::parseHipPolyline(QJsonArray {1, "not_hip"}).has_value());

    const auto parsedPolyline = StellariumHipParser::parseHipPolyline(QJsonArray {
        QJsonObject {{"hip", 1}},
        "hip_2",
        3
    });
    QVERIFY(parsedPolyline.has_value());
    QCOMPARE(*parsedPolyline, std::vector<int>({1, 2, 3}));

    std::vector<std::vector<int>> polylines;
    StellariumHipParser::collectHipPolylines(
        QJsonObject {
            {"constellations", QJsonArray {
                QJsonObject {{"lines", QJsonArray {QJsonArray {10, 11, 12}}}},
                QJsonObject {{"line", QJsonArray {QJsonArray {"hip 20", "hip_21"}}}},
                QJsonObject {{"nested", QJsonObject {{"segments", QJsonArray {QJsonArray {30, 31}}}}}},
            }}
        },
        polylines
    );

    QCOMPARE(polylines.size(), std::size_t(3));
    QCOMPARE(polylines[0], std::vector<int>({10, 11, 12}));
    QCOMPARE(polylines[1], std::vector<int>({20, 21}));
    QCOMPARE(polylines[2], std::vector<int>({30, 31}));
}

QTEST_APPLESS_MAIN(StellariumExtractorTests)

#include "StellariumExtractorTests.moc"
