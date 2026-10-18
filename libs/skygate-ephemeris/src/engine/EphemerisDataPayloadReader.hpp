#pragma once

#include "EphemerisDataManifest.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
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
        OutputLimitExceeded,
        Canceled
    };

    struct Result {
        Status status = Status::IoError;
        std::uint64_t outputBytes = 0U;
        std::string checksum;
        std::vector<std::string> diagnostics;
    };

    // Chunk size used while streaming payload bytes into a target device.
    static constexpr std::size_t kIoBufferBytes = 1U << 16U;

    [[nodiscard]] static bool cancellationRequested(const std::function<bool()>& callback);

    [[nodiscard]] static Result read(
        EphemerisDataManifest::CompressionKind compression,
        QFile& sourceFile,
        QIODevice& targetFile,
        const std::function<bool()>& cancellationCallback,
        std::optional<std::uint64_t> maximumOutputBytes = std::nullopt
    );
};

}  // namespace skygate::ephemeris
