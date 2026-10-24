#include "EphemerisManifestJsonReader.hpp"
#include "EphemerisDataFileUtilities.hpp"
#include "EphemerisEpochParsing.hpp"
#include "StringUtilities.hpp"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace skygate::ephemeris {

std::string EphemerisManifestJsonReader::requiredStringDiagnostic(const std::string_view context, const QString& key)
{
    return std::string(context) + " requires non-empty string field '"
           + EphemerisDataFileUtilities::stringFromQString(key) + "'.";
}

std::string EphemerisManifestJsonReader::fieldDiagnostic(
    const std::string_view context, const QString& key, const std::string_view requirement
)
{
    return std::string(context) + " field '" + EphemerisDataFileUtilities::stringFromQString(key) + "' "
           + std::string(requirement) + ".";
}

std::optional<skygate::core::AstronomicalEpoch>
EphemerisManifestJsonReader::parseUtcDateEpoch(const QString& text) noexcept
{
    return EphemerisEpochParsing::epochFromDateColumn(EphemerisDataFileUtilities::stringFromQString(text));
}

bool EphemerisManifestJsonReader::hasNonEmptyString(const QJsonObject& object, const QString& key) noexcept
{
    const QJsonValue value = object.value(key);
    return value.isString() && !value.toString().trimmed().isEmpty();
}

std::string EphemerisManifestJsonReader::stringValue(const QJsonObject& object, const QString& key)
{
    return EphemerisDataFileUtilities::stringFromQString(object.value(key).toString().trimmed());
}

bool EphemerisManifestJsonReader::readRequiredString(
    const QJsonObject& object,
    const QString& key,
    const std::string_view context,
    std::vector<std::string>& diagnostics,
    std::string& value
)
{
    if (!hasNonEmptyString(object, key)) {
        diagnostics.push_back(requiredStringDiagnostic(context, key));
        return false;
    }

    value = stringValue(object, key);
    return true;
}

bool EphemerisManifestJsonReader::readOptionalString(const QJsonObject& object, const QString& key, std::string& value)
{
    const QJsonValue jsonValue = object.value(key);
    if (!jsonValue.isString()) {
        return false;
    }

    value = EphemerisDataFileUtilities::stringFromQString(jsonValue.toString().trimmed());
    return !value.empty();
}

bool EphemerisManifestJsonReader::readRequiredBool(
    const QJsonObject& object,
    const QString& key,
    const std::string_view context,
    std::vector<std::string>& diagnostics,
    bool& value
)
{
    const QJsonValue jsonValue = object.value(key);
    if (!jsonValue.isBool()) {
        diagnostics.push_back(fieldDiagnostic(context, key, "must be a boolean"));
        return false;
    }

    value = jsonValue.toBool();
    return true;
}

bool EphemerisManifestJsonReader::readOptionalBool(
    const QJsonObject& object,
    const QString& key,
    const std::string_view context,
    std::vector<std::string>& diagnostics,
    bool& value
)
{
    const QJsonValue jsonValue = object.value(key);
    if (jsonValue.isUndefined()) {
        return true;
    }
    if (!jsonValue.isBool()) {
        diagnostics.push_back(fieldDiagnostic(context, key, "must be a boolean"));
        return false;
    }

    value = jsonValue.toBool();
    return true;
}

bool EphemerisManifestJsonReader::readUInt64(
    const QJsonObject& object,
    const QString& key,
    const std::string_view context,
    std::vector<std::string>& diagnostics,
    std::optional<std::uint64_t>& value
)
{
    const QJsonValue jsonValue = object.value(key);
    if (jsonValue.isUndefined()) {
        return true;
    }
    if (!jsonValue.isDouble()) {
        diagnostics.push_back(fieldDiagnostic(context, key, "must be an integer"));
        return false;
    }

    const double number = jsonValue.toDouble();
    if (!std::isfinite(number) || number < 0.0 || std::floor(number) != number) {
        diagnostics.push_back(fieldDiagnostic(context, key, "must be a non-negative integer"));
        return false;
    }
    if (number > static_cast<double>(std::numeric_limits<std::uint64_t>::max())) {
        diagnostics.push_back(fieldDiagnostic(context, key, "exceeds the uint64 range"));
        return false;
    }
    if (number > kMaxExactJsonInteger) {
        diagnostics.push_back(fieldDiagnostic(context, key, "exceeds the exact JSON integer range"));
        return false;
    }

    value = static_cast<std::uint64_t>(number);
    return true;
}

bool EphemerisManifestJsonReader::readRange(
    const QJsonObject& object,
    const QString& startKey,
    const QString& endKey,
    const std::string_view context,
    std::vector<std::string>& diagnostics,
    EphemerisDateRange& range
)
{
    bool valid = true;
    valid &= readRequiredString(object, "id", context, diagnostics, range.id);
    valid &= readRequiredString(object, "displayName", context, diagnostics, range.displayName);

    if (!hasNonEmptyString(object, startKey) || !hasNonEmptyString(object, endKey)) {
        diagnostics.push_back(
            std::string(context) + " requires non-empty '" + EphemerisDataFileUtilities::stringFromQString(startKey)
            + "' and '" + EphemerisDataFileUtilities::stringFromQString(endKey) + "' UTC date fields."
        );
        return false;
    }

    const std::optional<skygate::core::AstronomicalEpoch> start = parseUtcDateEpoch(object.value(startKey).toString());
    const std::optional<skygate::core::AstronomicalEpoch> end = parseUtcDateEpoch(object.value(endKey).toString());
    if (!start.has_value() || !end.has_value()) {
        diagnostics.push_back(std::string(context) + " contains malformed UTC date fields.");
        return false;
    }

    range.start = *start;
    range.end = *end;
    if (!range.isValid()) {
        diagnostics.push_back(std::string(context) + " must have an ordered finite UTC validity range.");
        return false;
    }

    return valid;
}

bool EphemerisManifestJsonReader::readDateRange(
    const QJsonObject& object,
    const std::string_view context,
    std::vector<std::string>& diagnostics,
    EphemerisDateRange& range
)
{
    return readRange(object, "start", "end", context, diagnostics, range);
}

bool EphemerisManifestJsonReader::readChecksum(
    const QJsonValue& value,
    const std::string_view context,
    std::vector<std::string>& diagnostics,
    EphemerisDataManifest::Checksum& checksum
)
{
    if (!value.isObject()) {
        diagnostics.push_back(std::string(context) + " requires checksum metadata.");
        return false;
    }

    const QJsonObject object = value.toObject();
    bool valid = true;
    valid &= readRequiredString(object, "algorithm", context, diagnostics, checksum.algorithm);
    valid &= readRequiredString(object, "value", context, diagnostics, checksum.value);
    return valid;
}

bool EphemerisManifestJsonReader::readCompression(
    const QJsonValue& value,
    const std::string_view context,
    std::vector<std::string>& diagnostics,
    EphemerisDataManifest::Compression& compression
)
{
    if (!value.isObject()) {
        diagnostics.push_back(std::string(context) + " requires compression metadata.");
        return false;
    }

    const QJsonObject object = value.toObject();
    std::string format;
    if (!readRequiredString(object, "format", context, diagnostics, format)) {
        return false;
    }

    const std::optional<EphemerisDataManifest::CompressionKind> parsedKind =
        EphemerisDataManifest::compressionKindFromText(format);
    if (!parsedKind.has_value()) {
        diagnostics.push_back(std::string(context) + " has unsupported compression format '" + format + "'.");
        return false;
    }
    compression.kind = *parsedKind;

    bool valid = true;
    valid &= readUInt64(object, "compressedSizeBytes", context, diagnostics, compression.compressedSizeBytes);
    valid &= readUInt64(object, "uncompressedSizeBytes", context, diagnostics, compression.uncompressedSizeBytes);
    if (compression.kind == EphemerisDataManifest::CompressionKind::Zstd
        && (!compression.compressedSizeBytes.has_value() || !compression.uncompressedSizeBytes.has_value()
            || *compression.compressedSizeBytes == 0U || *compression.uncompressedSizeBytes == 0U)) {
        diagnostics.push_back(
            std::string(context) + " zstd compression requires positive compressed and uncompressed sizes."
        );
        valid = false;
    }

    return valid;
}

bool EphemerisManifestJsonReader::readProfile(
    const QJsonValue& value, std::vector<std::string>& diagnostics, EphemerisDataManifest::Profile& profile
)
{
    if (!value.isObject()) {
        diagnostics.push_back("Manifest profile entries must be objects.");
        return false;
    }

    const QJsonObject object = value.toObject();
    const std::string context = "Profile";
    bool valid = true;
    valid &= readRequiredString(object, "id", context, diagnostics, profile.id);
    valid &= readRequiredString(object, "displayName", context, diagnostics, profile.displayName);
    valid &= readRequiredBool(object, "bundled", context, diagnostics, profile.bundled);
    valid &= readRequiredBool(object, "longRange", context, diagnostics, profile.longRange);

    const QJsonValue assetIdsValue = object.value("assetIds");
    if (!assetIdsValue.isArray() || assetIdsValue.toArray().isEmpty()) {
        diagnostics.push_back("Profile requires a non-empty assetIds array.");
        valid = false;
    } else {
        std::unordered_set<std::string> seenAssetIds;
        for (const QJsonValue& assetIdValue : assetIdsValue.toArray()) {
            if (!assetIdValue.isString() || assetIdValue.toString().trimmed().isEmpty()) {
                diagnostics.push_back("Profile assetIds entries must be non-empty strings.");
                valid = false;
                continue;
            }
            std::string assetId = EphemerisDataFileUtilities::stringFromQString(assetIdValue.toString().trimmed());
            if (!seenAssetIds.insert(assetId).second) {
                diagnostics.push_back("Profile '" + profile.id + "' contains duplicate asset id '" + assetId + "'.");
                valid = false;
                continue;
            }
            profile.assetIds.push_back(std::move(assetId));
        }
    }

    return valid;
}

bool EphemerisManifestJsonReader::readAsset(
    const QJsonValue& value, std::vector<std::string>& diagnostics, EphemerisDataManifest::Asset& asset
)
{
    if (!value.isObject()) {
        diagnostics.push_back("Manifest asset entries must be objects.");
        return false;
    }

    const QJsonObject object = value.toObject();
    const std::string context = "Asset";
    bool valid = true;
    valid &= readRequiredString(object, "id", context, diagnostics, asset.id);

    std::string kind;
    if (readRequiredString(object, "kind", context, diagnostics, kind)) {
        const std::optional<EphemerisDataManifest::AssetKind> parsedKind =
            EphemerisDataManifest::assetKindFromText(kind);
        if (!parsedKind.has_value()) {
            diagnostics.push_back("Asset has unsupported kind '" + kind + "'.");
            valid = false;
        } else {
            asset.kind = *parsedKind;
        }
    } else {
        valid = false;
    }

    valid &= readRequiredString(object, "profileId", context, diagnostics, asset.profileId);
    valid &= readRequiredString(object, "version", context, diagnostics, asset.version);
    valid &= readRequiredString(object, "sourceUrl", context, diagnostics, asset.sourceUrl);
    (void)readOptionalString(object, "relativePath", asset.relativePath);
    valid &= readOptionalBool(object, "optional", context, diagnostics, asset.optional);
    valid &= readOptionalBool(object, "live", context, diagnostics, asset.live);

    if (object.contains("checksum")) {
        valid &= readChecksum(object.value("checksum"), context, diagnostics, asset.checksum);
    } else if (!asset.live) {
        diagnostics.push_back("Asset requires checksum metadata.");
        valid = false;
    }
    valid &= readCompression(object.value("compression"), context, diagnostics, asset.compression);
    if (!object.value("validityRange").isObject()) {
        diagnostics.push_back("Asset requires validityRange metadata.");
        valid = false;
    } else {
        valid &= readDateRange(object.value("validityRange").toObject(), context, diagnostics, asset.validityRange);
    }

    return valid;
}

void EphemerisManifestJsonReader::validateReferences(
    const EphemerisDataManifest& manifest, std::vector<std::string>& diagnostics
)
{
    std::unordered_set<std::string> profileIds;
    for (const EphemerisDataManifest::Profile& profile : manifest.profiles) {
        if (!profileIds.insert(profile.id).second) {
            diagnostics.push_back("Manifest contains duplicate profile id '" + profile.id + "'.");
        }
    }

    std::unordered_set<std::string> assetIds;
    for (const EphemerisDataManifest::Asset& asset : manifest.assets) {
        if (!assetIds.insert(asset.id).second) {
            diagnostics.push_back("Manifest contains duplicate asset id '" + asset.id + "'.");
        }
        if (manifest.profile(asset.profileId) == nullptr) {
            diagnostics.push_back("Asset '" + asset.id + "' references unknown profile '" + asset.profileId + "'.");
        }
    }

    for (const EphemerisDataManifest::Profile& profile : manifest.profiles) {
        for (const std::string& assetId : profile.assetIds) {
            const EphemerisDataManifest::Asset* asset = manifest.asset(assetId);
            if (asset == nullptr) {
                diagnostics.push_back("Profile '" + profile.id + "' references unknown asset '" + assetId + "'.");
                continue;
            }
            if (asset->profileId != profile.id) {
                diagnostics.push_back(
                    "Profile '" + profile.id + "' references asset '" + assetId + "' owned by profile '"
                    + asset->profileId + "'."
                );
            }
        }
    }
}

bool EphemerisManifestJsonReader::readDataSetInfo(
    const QJsonObject& rootObject, std::vector<std::string>& diagnostics, EphemerisDatasetInfo& dataSetInfo
)
{
    bool valid = true;
    valid &= readRequiredString(rootObject, "id", "Manifest", diagnostics, dataSetInfo.id);
    valid &= readRequiredString(rootObject, "displayName", "Manifest", diagnostics, dataSetInfo.displayName);
    valid &= readRequiredString(rootObject, "version", "Manifest", diagnostics, dataSetInfo.version);
    valid &= readRequiredString(rootObject, "provenance", "Manifest", diagnostics, dataSetInfo.provenance);

    const QJsonValue dateRangesValue = rootObject.value("dateRanges");
    if (!dateRangesValue.isArray() || dateRangesValue.toArray().isEmpty()) {
        diagnostics.push_back("Manifest requires a non-empty dateRanges array.");
        valid = false;
    } else {
        for (const QJsonValue& dateRangeValue : dateRangesValue.toArray()) {
            if (!dateRangeValue.isObject()) {
                diagnostics.push_back("Manifest dateRanges entries must be objects.");
                valid = false;
                continue;
            }

            EphemerisDateRange range;
            if (readDateRange(dateRangeValue.toObject(), "Manifest date range", diagnostics, range)) {
                dataSetInfo.dateRanges.push_back(std::move(range));
            } else {
                valid = false;
            }
        }
    }

    return valid;
}

EphemerisDataManifest::ParseResult EphemerisManifestJsonReader::read(const std::string_view payload)
{
    EphemerisDataManifest::ParseResult result;
    if (StringUtilities::trimAsciiWhitespace(payload).empty()) {
        result.diagnostics.push_back("Manifest payload is empty.");
        return result;
    }

    QJsonParseError parseError;
    const QByteArray payloadBytes(payload.data(), static_cast<qsizetype>(payload.size()));
    const QJsonDocument document = QJsonDocument::fromJson(payloadBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        result.diagnostics.push_back("Manifest payload must be a valid JSON object.");
        return result;
    }

    const QJsonObject rootObject = document.object();
    const QJsonValue schemaVersionValue = rootObject.value("schemaVersion");
    if (!schemaVersionValue.isDouble() || schemaVersionValue.toInt() != kSupportedSchemaVersion) {
        result.diagnostics.push_back("Manifest schemaVersion must be " + std::to_string(kSupportedSchemaVersion) + ".");
    }
    result.manifest.schemaVersion = kSupportedSchemaVersion;

    bool valid = readDataSetInfo(rootObject, result.diagnostics, result.manifest.dataSetInfo);

    const QJsonValue profilesValue = rootObject.value("profiles");
    if (!profilesValue.isArray() || profilesValue.toArray().isEmpty()) {
        result.diagnostics.push_back("Manifest requires a non-empty profiles array.");
        valid = false;
    } else {
        for (const QJsonValue& profileValue : profilesValue.toArray()) {
            EphemerisDataManifest::Profile profile;
            if (readProfile(profileValue, result.diagnostics, profile)) {
                result.manifest.profiles.push_back(std::move(profile));
            } else {
                valid = false;
            }
        }
    }

    const QJsonValue assetsValue = rootObject.value("assets");
    if (!assetsValue.isArray() || assetsValue.toArray().isEmpty()) {
        result.diagnostics.push_back("Manifest requires a non-empty assets array.");
        valid = false;
    } else {
        for (const QJsonValue& assetValue : assetsValue.toArray()) {
            EphemerisDataManifest::Asset asset;
            if (readAsset(assetValue, result.diagnostics, asset)) {
                result.manifest.assets.push_back(std::move(asset));
            } else {
                valid = false;
            }
        }
    }

    const std::size_t diagnosticsBeforeReferenceValidation = result.diagnostics.size();
    validateReferences(result.manifest, result.diagnostics);
    valid = valid && diagnosticsBeforeReferenceValidation == result.diagnostics.size();

    if (valid && result.diagnostics.empty()) {
        result.status = EphemerisDataManifest::Status::Valid;
    }

    return result;
}

}  // namespace skygate::ephemeris
