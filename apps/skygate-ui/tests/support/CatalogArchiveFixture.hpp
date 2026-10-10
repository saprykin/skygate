#pragma once

#include "CatalogTestPayloads.hpp"

#include <QByteArray>
#include <QString>

namespace skygate::ui::tests {

// Builds the two-member archive whose members hold distinguishable objects, so
// a test can tell which member a restored source actually selected.
class CatalogArchiveFixture final {
public:
    [[nodiscard]] static QByteArray archiveMemberZip(const DeepSkyCatalogPayloadOptions& deepSky);
    [[nodiscard]] static QString starsMemberPath();
    [[nodiscard]] static QString deepSkyMemberPath();
};

}  // namespace skygate::ui::tests
