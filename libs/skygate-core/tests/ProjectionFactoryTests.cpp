#include "skygate/core/ProjectionFactory.hpp"

#include <QtTest/QtTest>

class ProjectionFactoryTests final : public QObject {
    Q_OBJECT

private slots:
    void supportedProjectionsAreCreated();
    void unsupportedProjectionReturnsNull();
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
}

void ProjectionFactoryTests::unsupportedProjectionReturnsNull()
{
    const auto unsupportedProjection =
        skygate::core::createProjection(skygate::core::ProjectionType::AzimuthalEquidistant);
    QVERIFY(unsupportedProjection == nullptr);
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
}

QTEST_APPLESS_MAIN(ProjectionFactoryTests)

#include "ProjectionFactoryTests.moc"
