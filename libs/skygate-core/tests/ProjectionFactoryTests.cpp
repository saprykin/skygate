#include "skygate/core/ProjectionFactory.hpp"

#include <QtTest/QtTest>

class ProjectionFactoryTests final : public QObject {
    Q_OBJECT

private slots:
    void supportedProjectionsAreCreated();
    void azimuthalProjectionIsCreated();
    void createdProjectionReportsItsType();
};

void ProjectionFactoryTests::supportedProjectionsAreCreated()
{
    const auto stereographicProjection =
        skygate::core::createProjection(skygate::core::ProjectionType::Stereographic);
    QVERIFY(stereographicProjection != nullptr);

    const auto perspectiveProjection =
        skygate::core::createProjection(skygate::core::ProjectionType::Perspective);
    QVERIFY(perspectiveProjection != nullptr);

    const auto azimuthalProjection =
        skygate::core::createProjection(skygate::core::ProjectionType::AzimuthalEquidistant);
    QVERIFY(azimuthalProjection != nullptr);
}

void ProjectionFactoryTests::azimuthalProjectionIsCreated()
{
    const auto azimuthalProjection =
        skygate::core::createProjection(skygate::core::ProjectionType::AzimuthalEquidistant);
    QVERIFY(azimuthalProjection != nullptr);
}

void ProjectionFactoryTests::createdProjectionReportsItsType()
{
    const auto stereographicProjection =
        skygate::core::createProjection(skygate::core::ProjectionType::Stereographic);
    QVERIFY(stereographicProjection != nullptr);
    QCOMPARE(stereographicProjection->type(), skygate::core::ProjectionType::Stereographic);

    const auto perspectiveProjection =
        skygate::core::createProjection(skygate::core::ProjectionType::Perspective);
    QVERIFY(perspectiveProjection != nullptr);
    QCOMPARE(perspectiveProjection->type(), skygate::core::ProjectionType::Perspective);

    const auto azimuthalProjection =
        skygate::core::createProjection(skygate::core::ProjectionType::AzimuthalEquidistant);
    QVERIFY(azimuthalProjection != nullptr);
    QCOMPARE(azimuthalProjection->type(), skygate::core::ProjectionType::AzimuthalEquidistant);
}

QTEST_APPLESS_MAIN(ProjectionFactoryTests)

#include "ProjectionFactoryTests.moc"
