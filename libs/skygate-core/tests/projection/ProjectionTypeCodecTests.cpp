#include "ProjectionType.hpp"
#include "projections/ProjectionTypeCodec.hpp"

#include <QtTest/QtTest>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class ProjectionTypeCodecTests final : public QObject {
    Q_OBJECT

private slots:
    void orderedValuesMatchDisplayOrder();
    void isKnownRejectsValuesOutsideTheEnum();
    void textRoundTripsEveryValue();
    void fromTextAcceptsCaseAndWhitespace();
    void fromTextRejectsUnknownText();
    void unknownValueHasNoText();
};

void ProjectionTypeCodecTests::orderedValuesMatchDisplayOrder()
{
    const std::vector<skygate::core::ProjectionType> values = skygate::core::ProjectionTypeCodec::orderedValues();
    QCOMPARE(values.size(), std::size_t{3});
    QCOMPARE(values[0], skygate::core::ProjectionType::Stereographic);
    QCOMPARE(values[1], skygate::core::ProjectionType::AzimuthalEquidistant);
    QCOMPARE(values[2], skygate::core::ProjectionType::Perspective);

    QCOMPARE(
        QString::fromStdString(std::string(skygate::core::ProjectionTypeCodec::text(values[0]))),
        QString("Stereographic")
    );
    QCOMPARE(
        QString::fromStdString(std::string(skygate::core::ProjectionTypeCodec::text(values[1]))),
        QString("AzimuthalEquidistant")
    );
    QCOMPARE(
        QString::fromStdString(std::string(skygate::core::ProjectionTypeCodec::text(values[2]))), QString("Perspective")
    );
}

void ProjectionTypeCodecTests::isKnownRejectsValuesOutsideTheEnum()
{
    QVERIFY(!skygate::core::ProjectionTypeCodec::isKnown(static_cast<skygate::core::ProjectionType>(3)));
    QVERIFY(!skygate::core::ProjectionTypeCodec::isKnown(static_cast<skygate::core::ProjectionType>(255)));
}

void ProjectionTypeCodecTests::textRoundTripsEveryValue()
{
    for (const skygate::core::ProjectionType value : skygate::core::ProjectionTypeCodec::orderedValues()) {
        const std::string_view text = skygate::core::ProjectionTypeCodec::text(value);
        QVERIFY(!text.empty());

        const std::optional<skygate::core::ProjectionType> parsed = skygate::core::ProjectionTypeCodec::fromText(text);
        QVERIFY(parsed.has_value());
        QCOMPARE(*parsed, value);
    }
}

void ProjectionTypeCodecTests::fromTextAcceptsCaseAndWhitespace()
{
    const std::optional<skygate::core::ProjectionType> stereographic =
        skygate::core::ProjectionTypeCodec::fromText("  STEREOGRAPHIC\t");
    QVERIFY(stereographic.has_value());
    QCOMPARE(*stereographic, skygate::core::ProjectionType::Stereographic);

    const std::optional<skygate::core::ProjectionType> azimuthal =
        skygate::core::ProjectionTypeCodec::fromText("azimuthalequidistant");
    QVERIFY(azimuthal.has_value());
    QCOMPARE(*azimuthal, skygate::core::ProjectionType::AzimuthalEquidistant);
}

void ProjectionTypeCodecTests::fromTextRejectsUnknownText()
{
    QVERIFY(!skygate::core::ProjectionTypeCodec::fromText("").has_value());
    QVERIFY(!skygate::core::ProjectionTypeCodec::fromText("FishEyePrototype").has_value());
    QVERIFY(!skygate::core::ProjectionTypeCodec::fromText("Stereo graphic").has_value());
}

void ProjectionTypeCodecTests::unknownValueHasNoText()
{
    const auto unknownValue = static_cast<skygate::core::ProjectionType>(200);
    QVERIFY(skygate::core::ProjectionTypeCodec::text(unknownValue).empty());
    QVERIFY(!skygate::core::ProjectionTypeCodec::fromText(skygate::core::ProjectionTypeCodec::text(unknownValue))
                 .has_value());
}

QTEST_APPLESS_MAIN(ProjectionTypeCodecTests)

#include "ProjectionTypeCodecTests.moc"
