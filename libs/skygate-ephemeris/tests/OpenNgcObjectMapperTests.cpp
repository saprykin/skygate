#include "catalog/OpenNgcObjectMapper.hpp"

#include <QtTest/QtTest>

#include <algorithm>
#include <string>

class OpenNgcObjectMapperTests final : public QObject {
    Q_OBJECT

private slots:
    void skipsUnsupportedObjectTypes();
    void mapsMessierObjectsAndAliases();
};

void OpenNgcObjectMapperTests::skipsUnsupportedObjectTypes()
{
    QVERIFY(skygate::ephemeris::OpenNgcObjectMapper::shouldSkipType(QStringLiteral("Star")));
}

void OpenNgcObjectMapperTests::mapsMessierObjectsAndAliases()
{
    using namespace skygate::ephemeris;

    const auto mapping = OpenNgcObjectMapper::mapObject(
        QStringLiteral("G"),
        QStringLiteral("NGC0224"),
        OpenNgcObjectMapper::withoutLeadingZeros(QStringLiteral("031")),
        OpenNgcObjectMapper::withoutLeadingZeros(QStringLiteral("0224")),
        {},
        QStringLiteral("NGC0224,M 031"),
        QStringLiteral("Andromeda Galaxy")
    );

    QCOMPARE(mapping.id, std::string("messier_031"));
    QCOMPARE(mapping.displayName, std::string("M31"));
    QCOMPARE(mapping.kind, DeepSkyObjectKind::Galaxy);
    QVERIFY(
        std::find(mapping.aliases.begin(), mapping.aliases.end(), "M 31")
        != mapping.aliases.end()
    );
    QVERIFY(
        std::find(mapping.aliases.begin(), mapping.aliases.end(), "NGC 224")
        != mapping.aliases.end()
    );
    QVERIFY(
        std::find(mapping.aliases.begin(), mapping.aliases.end(), "Andromeda Galaxy")
        != mapping.aliases.end()
    );
}

QTEST_APPLESS_MAIN(OpenNgcObjectMapperTests)

#include "OpenNgcObjectMapperTests.moc"
