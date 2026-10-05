#pragma once

#include <cstdint>

namespace skygate::ephemeris {

// Schema identifier of a catalog source.
//
// A schema defines how a decoded plain-text payload maps to catalog bodies;
// archive containers (gzip, ZIP) are decoded before schema dispatch and are
// not source types. The registered schemas, their required columns,
// delimiters, and parser factories live in CatalogSchemaRegistry. Unknown is
// the value used for "no schema detected yet" and for unsupported payloads.
enum class CatalogSourceType : std::uint8_t {
    Bundled,
    HygCsv,
    OpenNgcCsv,
    Unknown
};

}  // namespace skygate::ephemeris
