#pragma once

#include "EquatorialCoordinate.hpp"

#include <QByteArray>
#include <QDataStream>

#include <cstdint>
#include <memory>
#include <optional>

namespace skygate::ephemeris {

class BaseCelestialBody;
class CelestialBodyCatalog;
class IStarCatalog;

/// Serializes parsed star catalogs to a compact binary cache payload and
/// reads them back, so catalog-cache restores skip the gzip CSV re-parse on
/// every startup.
///
/// This is the model-owned encoding/decoding boundary for the immutable
/// catalog snapshot. Body field layouts and validated catalog construction
/// live here rather than in the UI cache layer.
///
/// Payloads start with the binary catalog magic and the schema version.
/// deserialize accepts only kSchemaVersion and rejects anything else, so
/// caches written by older development builds fall back to raw catalog
/// parsing in the UI cache layer.
class CatalogBinaryCodec final {
public:
    static constexpr std::uint16_t kSchemaVersion = 1U;

    [[nodiscard]] static QByteArray serialize(const CelestialBodyCatalog& catalog);
    [[nodiscard]] static std::unique_ptr<IStarCatalog> deserialize(const QByteArray& payload);

private:
    static void writeBodyHeader(
        QDataStream& stream,
        const BaseCelestialBody& body,
        const std::optional<skygate::core::EquatorialCoordinate>& fixedEquatorial
    );
    [[nodiscard]] static bool readBodyHeader(
        QDataStream& stream,
        BaseCelestialBody& body,
        std::optional<skygate::core::EquatorialCoordinate>& fixedEquatorial
    );
};

}  // namespace skygate::ephemeris
