#pragma once

#include <cstdint>

namespace skygate::ephemeris {

/// Loading outcome of one ephemeris table payload. The table providers share
/// the outcomes and state separately which of them still carry usable data.
enum class EphemerisTableDataStatus : std::uint8_t {
    Available,
    Missing,
    Malformed,
    Stale
};

}  // namespace skygate::ephemeris
