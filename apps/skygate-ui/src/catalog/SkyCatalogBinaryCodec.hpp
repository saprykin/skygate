#pragma once

#include "catalog/IStarCatalog.hpp"

#include <QByteArray>

#include <cstdint>
#include <memory>

namespace skygate::ephemeris {
class CelestialBodyCatalog;
}

namespace skygate::ui::internal {

/// Serializes parsed star catalogs to a compact binary cache payload and
/// reads them back, so catalog-cache restores skip the gzip CSV re-parse on
/// every startup.
class SkyCatalogBinaryCodec final {
public:
    static constexpr std::uint16_t kSchemaVersion = 2U;

    [[nodiscard]] static QByteArray serialize(const skygate::ephemeris::CelestialBodyCatalog& catalog);
    [[nodiscard]] static std::unique_ptr<skygate::ephemeris::IStarCatalog> deserialize(const QByteArray& payload);
};

}  // namespace skygate::ui::internal
