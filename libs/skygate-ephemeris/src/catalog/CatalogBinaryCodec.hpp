#pragma once

#include <QByteArray>

#include <cstdint>
#include <memory>

namespace skygate::ephemeris {

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
/// Schema version 4 added the source-local identity scope marker of
/// CatalogObjectIdentity (version 3 added the per-body identity itself:
/// source record id, namespaced external identifiers, and display aliases).
/// Legacy payloads are rejected by deserialize and fall back to raw payload
/// parsing in the UI cache layer.
class CatalogBinaryCodec final {
public:
    static constexpr std::uint16_t kSchemaVersion = 4U;

    [[nodiscard]] static QByteArray serialize(const CelestialBodyCatalog& catalog);
    [[nodiscard]] static std::unique_ptr<IStarCatalog> deserialize(const QByteArray& payload);
};

}  // namespace skygate::ephemeris
