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

void StellariumExtractorTests::parsesStrictHipText()
{
    const auto hip = skygate::ephemeris::StellariumHipParser::parseStrictHipText("hip_24436");

    QVERIFY(hip.has_value());
    QCOMPARE(*hip, 24436);
}

QTEST_APPLESS_MAIN(StellariumExtractorTests)

#include "StellariumExtractorTests.moc"
