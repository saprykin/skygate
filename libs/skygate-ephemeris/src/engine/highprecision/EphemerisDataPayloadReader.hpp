#pragma once

#include "EphemerisDataManifest.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

class QFile;
class QIODevice;

namespace skygate::ephemeris {

class EphemerisDataPayloadReader final {
public:
    enum class Status : std::uint8_t {
        Read,
        UnsupportedCompression,
        CorruptArchive,
        IoError,
        Canceled
    };

    struct Result {
        Status status = Status::IoError;
        std::uint64_t outputBytes = 0U;
        std::string checksum;
        std::vector<std::string> diagnostics;
    };

    [[nodiscard]] static Result read(
        EphemerisDataManifest::CompressionKind compression,
        QFile& sourceFile,
        QIODevice& targetFile,
        const std::function<bool()>& cancellationCallback
    );
};

}  // namespace skygate::ephemeris
