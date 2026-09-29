#pragma once

#include <cstdint>

namespace skygate::ephemeris {

enum class EphemerisStagedUpdateVerificationStatus : std::uint8_t {
    Verified,
    InvalidRequest,
    UnsupportedProfile,
    IncompleteUpdateSet,
    WrongComponentKind,
    MissingAsset,
    MalformedMetadata,
    MismatchedMetadata,
    UnsupportedCompression,
    CorruptArchive,
    ChecksumMismatch,
    IoError,
    Canceled
};

}  // namespace skygate::ephemeris
