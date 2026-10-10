#pragma once

#include "SkyCatalogSourceInstance.hpp"

#include <QByteArray>
#include <QList>
#include <QString>

namespace skygate::ui::tests {

// Builds source instances and payloads for owner-bound related constellation
// dataset scenarios.
class RelatedDatasetFixture final {
public:
    [[nodiscard]] static skygate::ui::internal::SkyCatalogSourceInstance
    relatedDatasetInstance(const QString& catalogUrl, const QString& relatedUrl);
    [[nodiscard]] static QByteArray relatedDatasetPayload(const QString& constellationId, const QList<int>& hips);
    [[nodiscard]] static QByteArray orionRelatedDatasetPayload();
    [[nodiscard]] static QByteArray lyraRelatedDatasetPayload();
};

}  // namespace skygate::ui::tests
