#include "ProjectionType.hpp"
#include "projections/ProjectionTypeCodec.hpp"

#include <QtTest/QtTest>

#include <cstddef>
#include <vector>

class ProjectionFactoryTests final : public QObject {
    Q_OBJECT

private slots:
    void everyConcreteTypeIsKnown();
    void everyCodecValueIsKnown();
    void valuesOutsideTheEnumAreRejected();
};

void ProjectionFactoryTests::everyConcreteTypeIsKnown()
{
    QVERIFY(skygate::core::ProjectionTypeCodec::isKnown(skygate::core::ProjectionType::Stereographic));
    QVERIFY(skygate::core::ProjectionTypeCodec::isKnown(skygate::core::ProjectionType::AzimuthalEquidistant));
    QVERIFY(skygate::core::ProjectionTypeCodec::isKnown(skygate::core::ProjectionType::Perspective));
}

void ProjectionFactoryTests::everyCodecValueIsKnown()
{
    const std::vector<skygate::core::ProjectionType> values = skygate::core::ProjectionTypeCodec::orderedValues();
    QCOMPARE(values.size(), std::size_t{3});
    for (const skygate::core::ProjectionType value : values) {
        QVERIFY(skygate::core::ProjectionTypeCodec::isKnown(value));
    }
}

void ProjectionFactoryTests::valuesOutsideTheEnumAreRejected()
{
    QVERIFY(!skygate::core::ProjectionTypeCodec::isKnown(static_cast<skygate::core::ProjectionType>(3)));
    QVERIFY(!skygate::core::ProjectionTypeCodec::isKnown(static_cast<skygate::core::ProjectionType>(200)));
    QVERIFY(!skygate::core::ProjectionTypeCodec::isKnown(static_cast<skygate::core::ProjectionType>(255)));
}

QTEST_APPLESS_MAIN(ProjectionFactoryTests)

#include "ProjectionFactoryTests.moc"
