#include "RelatedDatasetFixture.hpp"

#include "ConstellationTestSupport.hpp"

namespace skygate::ui::tests {

skygate::ui::internal::SkyCatalogSourceInstance
RelatedDatasetFixture::relatedDatasetInstance(const QString& catalogUrl, const QString& relatedUrl)
{
    skygate::ui::internal::SkyCatalogSourceInstance instance =
        skygate::ui::internal::SkyCatalogSourceInstance::createCustom(catalogUrl);
    instance.relatedDatasetUrls = QStringList{relatedUrl};
    return instance;
}

QByteArray RelatedDatasetFixture::relatedDatasetPayload(const QString& constellationId, const QList<int>& hips)
{
    return skygate::ui::tests::stellariumConstellationIndexJsonPayload({{constellationId, hips}});
}

QByteArray RelatedDatasetFixture::orionRelatedDatasetPayload()
{
    return relatedDatasetPayload(QStringLiteral("orion"), {27989, 25336, 25930});
}

QByteArray RelatedDatasetFixture::lyraRelatedDatasetPayload()
{
    return relatedDatasetPayload(QStringLiteral("lyra"), {26311, 26727, 24436});
}

}  // namespace skygate::ui::tests
