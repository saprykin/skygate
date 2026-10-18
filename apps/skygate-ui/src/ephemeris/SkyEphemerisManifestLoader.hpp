#pragma once

#include "engine/EphemerisDataManifest.hpp"

#include <QString>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace skygate::ui::internal {

// Reads and parses an ephemeris data manifest for the startup data-root
// search and the staged update workflow. A caller can enforce a maximum size
// for downloaded manifests; bundled files can be read without a cap.
class SkyEphemerisManifestLoader final {
public:
    enum class Status : std::uint8_t {
        Loaded,
        Missing,
        OpenFailed,
        TooLarge,
        ParseFailed,
    };

    struct Result final {
        Status status = Status::Missing;
        skygate::ephemeris::EphemerisDataManifest manifest;
        std::vector<std::string> diagnostics;
        QString fileErrorText;
        qint64 byteCount = 0;

        [[nodiscard]] bool isSuccess() const noexcept
        {
            return status == Status::Loaded;
        }
    };

    static constexpr std::uint64_t kDefaultMaximumBytes = 4ULL * 1024ULL * 1024ULL;

    SkyEphemerisManifestLoader() = delete;

    [[nodiscard]] static Result
    load(const QString& path, std::optional<std::uint64_t> maximumBytes = kDefaultMaximumBytes);
};

}  // namespace skygate::ui::internal
