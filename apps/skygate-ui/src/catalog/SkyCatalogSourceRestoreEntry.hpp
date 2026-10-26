#pragma once

#include "SkyCatalogSourceInstance.hpp"
#include "SkyCatalogSourceRecord.hpp"

#include <QByteArray>

namespace skygate::ui::internal {

struct SkyCatalogSourceRestoreEntry final {
    SkyCatalogSourceRecord record;
    SkyCatalogSourceInstance instance;
    QByteArray payload;
    bool requiresBinaryUpgrade = false;
};

}  // namespace skygate::ui::internal
