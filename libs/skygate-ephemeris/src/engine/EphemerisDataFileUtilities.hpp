#pragma once

#include "EphemerisDataManifest.hpp"
#include "EphemerisDataPayloadReader.hpp"

#include <QString>

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>

class QFileInfo;

namespace skygate::ephemeris {

class EphemerisDataFileUtilities final {
public:
    EphemerisDataFileUtilities() = delete;

    static constexpr std::string_view kSha256Algorithm = "sha256";

    [[nodiscard]] static QString pathToQString(const std::filesystem::path& path);
    [[nodiscard]] static std::filesystem::path pathFromQString(const QString& path);
    [[nodiscard]] static QString stringToQString(const std::string& value);
    [[nodiscard]] static bool hasUnsafePathComponent(const std::filesystem::path& path);
    [[nodiscard]] static bool
    sourceSizeMatchesMetadata(const QFileInfo& sourceInfo, const EphemerisDataManifest::Asset& asset);
    [[nodiscard]] static bool cancellationRequested(const std::function<bool()>& callback);
    [[nodiscard]] static bool checksumsMatch(std::string_view actual, std::string_view expected) noexcept;

    template <typename TargetStatus>
    [[nodiscard]] static TargetStatus payloadStatus(const EphemerisDataPayloadReader::Status status) noexcept
    {
        switch (status) {
        case EphemerisDataPayloadReader::Status::UnsupportedCompression:
            return TargetStatus::UnsupportedCompression;
        case EphemerisDataPayloadReader::Status::CorruptArchive:
            return TargetStatus::CorruptArchive;
        case EphemerisDataPayloadReader::Status::Canceled:
            return TargetStatus::Canceled;
        case EphemerisDataPayloadReader::Status::IoError:
            return TargetStatus::IoError;
        case EphemerisDataPayloadReader::Status::OutputLimitExceeded:
            return TargetStatus::ChecksumMismatch;
        case EphemerisDataPayloadReader::Status::Read:
            break;
        }

        return TargetStatus::IoError;
    }
};

}  // namespace skygate::ephemeris
