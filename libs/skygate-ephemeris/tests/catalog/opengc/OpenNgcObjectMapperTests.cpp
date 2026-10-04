#include "catalog/opengc/OpenNgcObjectMapper.hpp"

#include <QtTest/QtTest>

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace {

bool hasIdentifier(
    const std::vector<skygate::ephemeris::CatalogIdentifier>& identifiers,
    const std::string_view namespaceName,
    const std::string_view value
)
{
    return std::any_of(identifiers.begin(), identifiers.end(), [&](const skygate::ephemeris::CatalogIdentifier& id) {
        return id.namespaceName == namespaceName && id.value == value;
    });
}

bool hasAlias(const std::vector<std::string>& aliases, const std::string_view alias)
{
    return std::find(aliases.begin(), aliases.end(), alias) != aliases.end();
}

}  // namespace

class OpenNgcObjectMapperTests final : public QObject {
    Q_OBJECT

private slots:
    void skipsUnsupportedObjectTypes();
    void mapsMessierObjectsAndAliases();
    void extractsDesignationFromPrimaryName();
    void extractsSuffixedDesignationFromPrimaryName();
    void matchesCaseAndPaddingDesignationVariants();
    void ignoresUnrecognizedPrimaryNames();
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
    QCOMPARE(mapping.kind, DeepSkyObjectInfo::Kind::Galaxy);
    QVERIFY(std::find(mapping.aliases.begin(), mapping.aliases.end(), "M 31") != mapping.aliases.end());
    QVERIFY(std::find(mapping.aliases.begin(), mapping.aliases.end(), "NGC 224") != mapping.aliases.end());
    QVERIFY(std::find(mapping.aliases.begin(), mapping.aliases.end(), "Andromeda Galaxy") != mapping.aliases.end());
    QVERIFY(hasIdentifier(mapping.externalIdentifiers, "messier", "031"));
    QVERIFY(hasIdentifier(mapping.externalIdentifiers, "ngc", "224"));
}

void OpenNgcObjectMapperTests::extractsDesignationFromPrimaryName()
{
    using namespace skygate::ephemeris;

    const auto mapping = OpenNgcObjectMapper::mapObject(
        QStringLiteral("G"), QStringLiteral("NGC0001"), {}, {}, {}, {}, QStringLiteral("Shared region")
    );

    QCOMPARE(mapping.id, std::string("ngc_1"));
    QCOMPARE(mapping.displayName, std::string("NGC 1"));
    QVERIFY(hasIdentifier(mapping.externalIdentifiers, "ngc", "1"));
    QVERIFY(hasAlias(mapping.aliases, "NGC 1"));
    QVERIFY(hasAlias(mapping.aliases, "Shared region"));
}

void OpenNgcObjectMapperTests::extractsSuffixedDesignationFromPrimaryName()
{
    using namespace skygate::ephemeris;

    const auto ngcSuffixed =
        OpenNgcObjectMapper::mapObject(QStringLiteral("G"), QStringLiteral("NGC1234A"), {}, {}, {}, {}, {});
    QCOMPARE(ngcSuffixed.id, std::string("ngc_1234A"));
    QVERIFY(hasIdentifier(ngcSuffixed.externalIdentifiers, "ngc", "1234a"));
    QVERIFY(!hasIdentifier(ngcSuffixed.externalIdentifiers, "ngc", "1234"));

    const auto icSuffixed =
        OpenNgcObjectMapper::mapObject(QStringLiteral("Neb"), QStringLiteral("IC 4715b"), {}, {}, {}, {}, {});
    QCOMPARE(icSuffixed.id, std::string("ic_4715b"));
    QVERIFY(hasIdentifier(icSuffixed.externalIdentifiers, "ic", "4715b"));
}

void OpenNgcObjectMapperTests::matchesCaseAndPaddingDesignationVariants()
{
    using namespace skygate::ephemeris;

    const auto padded = OpenNgcObjectMapper::mapObject(
        QStringLiteral("G"),
        QStringLiteral("NGC0001"),
        {},
        OpenNgcObjectMapper::withoutLeadingZeros(QStringLiteral("0001")),
        {},
        {},
        {}
    );
    const auto lowered =
        OpenNgcObjectMapper::mapObject(QStringLiteral("G"), QStringLiteral("ngc 0001"), {}, {}, {}, {}, {});

    QVERIFY(!padded.externalIdentifiers.empty());
    QCOMPARE(padded.externalIdentifiers.size(), lowered.externalIdentifiers.size());
    for (std::size_t index = 0; index < padded.externalIdentifiers.size(); ++index) {
        QCOMPARE(
            QString::fromStdString(padded.externalIdentifiers[index].key()),
            QString::fromStdString(lowered.externalIdentifiers[index].key())
        );
    }
}

void OpenNgcObjectMapperTests::ignoresUnrecognizedPrimaryNames()
{
    using namespace skygate::ephemeris;

    const auto mapping = OpenNgcObjectMapper::mapObject(
        QStringLiteral("G"), QStringLiteral("PGC 2557"), {}, {}, {}, {}, QStringLiteral("Andromeda Galaxy")
    );

    QVERIFY(mapping.externalIdentifiers.empty());
    QVERIFY(mapping.id.starts_with("open_ngc_"));
}

QTEST_APPLESS_MAIN(OpenNgcObjectMapperTests)

#include "OpenNgcObjectMapperTests.moc"
