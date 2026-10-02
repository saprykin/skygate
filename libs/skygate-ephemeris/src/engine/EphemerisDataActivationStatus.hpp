#pragma once

#include <cstdint>

namespace skygate::ephemeris {

enum class EphemerisDataActivationStatus : std::uint8_t {
    Activated,
    AlreadyActive,
    InvalidRequest,
    MissingSource,
    UnsupportedCompression,
    CorruptArchive,
    ChecksumMismatch,
    IoError,
    LargeKernelInQtResource,
    Canceled
};

}  // namespace skygate::ephemeris
