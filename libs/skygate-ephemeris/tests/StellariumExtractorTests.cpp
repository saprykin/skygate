#include "catalog/StellariumHipParser.hpp"
#include "catalog/StellariumLabelRefExtractor.hpp"
#include "catalog/StellariumLineRefExtractor.hpp"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest/QtTest>

#include <string>
#include <string_view>

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
    const auto hip = skygate::ephemeris::StellariumHipParser::parseStrictHipText("hip_24436");

    QVERIFY(hip.has_value());
    QCOMPARE(*hip, 24436);
}

QTEST_APPLESS_MAIN(StellariumExtractorTests)

#include "StellariumExtractorTests.moc"
