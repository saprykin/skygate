#include "CatalogArchiveFixture.hpp"

#include "CatalogArchiveTestSupport.hpp"

#include <string>

namespace skygate::ui::tests {

QByteArray CatalogArchiveFixture::archiveMemberZip(const DeepSkyCatalogPayloadOptions& deepSky)
{
    const QByteArray starsMember =
        sampleHygCsvPayload({.hip = 900101, .properName = "Archive Member Star", .mag = "1.0"});
    const QByteArray deepSkyMember = sampleOpenNgcCsvPayload(deepSky);

    const std::string zipData = skygate::ephemeris::tests::makeZip({
        skygate::ephemeris::tests::ZipEntrySpec{
            .path = starsMemberPath().toStdString(), .data = starsMember.toStdString()
        },
        skygate::ephemeris::tests::ZipEntrySpec{
            .path = deepSkyMemberPath().toStdString(), .data = deepSkyMember.toStdString()
        },
    });
    return QByteArray(zipData.data(), static_cast<qsizetype>(zipData.size()));
}

QString CatalogArchiveFixture::starsMemberPath()
{
    return QStringLiteral("catalog/stars.csv");
}

QString CatalogArchiveFixture::deepSkyMemberPath()
{
    return QStringLiteral("catalog/deep-sky.csv");
}

}  // namespace skygate::ui::tests
