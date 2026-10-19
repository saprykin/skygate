#pragma once

#include "EphemerisDataManifest.hpp"
#include "time/AstronomicalEpoch.hpp"
#include "time/EphemerisDateRange.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class QJsonObject;
class QJsonValue;
class QString;

namespace skygate::ephemeris {

// Turns a manifest JSON payload into manifest values and reports the field
// problems it finds as diagnostics.
class EphemerisManifestJsonReader final {
public:
    EphemerisManifestJsonReader() = delete;

    static constexpr int kSupportedSchemaVersion = 1;

    [[nodiscard]] static EphemerisDataManifest::ParseResult read(const std::string_view payload);

private:
    static constexpr double kMaxExactJsonInteger = 9007199254740991.0;

    [[nodiscard]] static std::string requiredStringDiagnostic(const std::string_view context, const QString& key);
    [[nodiscard]] static std::string
    fieldDiagnostic(const std::string_view context, const QString& key, const std::string_view requirement);

    [[nodiscard]] static std::optional<skygate::core::AstronomicalEpoch>
    parseUtcDateEpoch(const QString& text) noexcept;
    [[nodiscard]] static bool hasNonEmptyString(const QJsonObject& object, const QString& key) noexcept;
    [[nodiscard]] static std::string stringValue(const QJsonObject& object, const QString& key);

    [[nodiscard]] static bool readRequiredString(
        const QJsonObject& object,
        const QString& key,
        const std::string_view context,
        std::vector<std::string>& diagnostics,
        std::string& value
    );
    [[nodiscard]] static bool readOptionalString(const QJsonObject& object, const QString& key, std::string& value);
    [[nodiscard]] static bool readRequiredBool(
        const QJsonObject& object,
        const QString& key,
        const std::string_view context,
        std::vector<std::string>& diagnostics,
        bool& value
    );
    [[nodiscard]] static bool readOptionalBool(
        const QJsonObject& object,
        const QString& key,
        const std::string_view context,
        std::vector<std::string>& diagnostics,
        bool& value
    );
    [[nodiscard]] static bool readUInt64(
        const QJsonObject& object,
        const QString& key,
        const std::string_view context,
        std::vector<std::string>& diagnostics,
        std::optional<std::uint64_t>& value
    );
    [[nodiscard]] static bool readRange(
        const QJsonObject& object,
        const QString& startKey,
        const QString& endKey,
        const std::string_view context,
        std::vector<std::string>& diagnostics,
        EphemerisDateRange& range
    );
    [[nodiscard]] static bool readDateRange(
        const QJsonObject& object,
        const std::string_view context,
        std::vector<std::string>& diagnostics,
        EphemerisDateRange& range
    );
    [[nodiscard]] static bool readChecksum(
        const QJsonValue& value,
        const std::string_view context,
        std::vector<std::string>& diagnostics,
        EphemerisDataManifest::Checksum& checksum
    );
    [[nodiscard]] static bool readCompression(
        const QJsonValue& value,
        const std::string_view context,
        std::vector<std::string>& diagnostics,
        EphemerisDataManifest::Compression& compression
    );
    [[nodiscard]] static bool readProfile(
        const QJsonValue& value, std::vector<std::string>& diagnostics, EphemerisDataManifest::Profile& profile
    );
    [[nodiscard]] static bool
    readAsset(const QJsonValue& value, std::vector<std::string>& diagnostics, EphemerisDataManifest::Asset& asset);
    [[nodiscard]] static bool readDataSetInfo(
        const QJsonObject& rootObject, std::vector<std::string>& diagnostics, EphemerisDatasetInfo& dataSetInfo
    );
    static void validateReferences(const EphemerisDataManifest& manifest, std::vector<std::string>& diagnostics);
};

}  // namespace skygate::ephemeris
