#pragma once

#include "EphemerisDataManifest.hpp"
#include "EphemerisDataPayloadReader.hpp"

#include <QString>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>

class QFileInfo;

namespace skygate::ephemeris {

class EphemerisDataFileUtilities final {
public:
    // Outcome of comparing decoded payload bytes against the manifest entry
    // that describes them.
    enum class PayloadVerification : std::uint8_t {
        Verified,
        EmptyLivePayload,
        UncompressedSizeMismatch,
        ChecksumMismatch
    };

    // Wording that a reporting path uses for decoded payload diagnostics. The
    // activity and subject name the asset, for example "Staged ephemeris
    // asset", and the size detail is the size wording that path reports.
    struct PayloadVerificationWording {
        std::string_view activity;
        std::string_view subject;
        std::string_view sizeDetail;
    };

    EphemerisDataFileUtilities() = delete;

    static constexpr std::string_view kSha256Algorithm = "sha256";

    static constexpr PayloadVerificationWording kStagedPayloadWording{
        .activity = "Staged",
        .subject = "ephemeris asset",
        .sizeDetail = "uncompressed size",
    };

    static constexpr PayloadVerificationWording kActivatedPayloadWording{
        .activity = "Activated",
        .subject = "ephemeris data asset",
        .sizeDetail = "size",
    };

    [[nodiscard]] static QString pathToQString(const std::filesystem::path& path);
    [[nodiscard]] static std::filesystem::path pathFromQString(const QString& path);
    [[nodiscard]] static QString stringToQString(const std::string& value);
    [[nodiscard]] static std::string stringFromQString(const QString& value)
    {
        return value.toStdString();
    }
    [[nodiscard]] static bool hasUnsafePathComponent(const std::filesystem::path& path);
    [[nodiscard]] static bool
    sourceSizeMatchesMetadata(const QFileInfo& sourceInfo, const EphemerisDataManifest::Asset& asset);
    // Checks the decoded size and digest that a staged update or an activation
    // reads back against the manifest metadata of the same asset.
    [[nodiscard]] static PayloadVerification verifyDecodedPayload(
        const EphemerisDataManifest::Asset& asset, std::uint64_t outputBytes, std::string_view decodedChecksum
    ) noexcept;
    [[nodiscard]] static std::string
    payloadVerificationDiagnostic(PayloadVerification verification, const PayloadVerificationWording& wording);
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
