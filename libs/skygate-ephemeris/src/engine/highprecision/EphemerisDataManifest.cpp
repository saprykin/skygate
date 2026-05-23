#include "time/CalendarTime.hpp"
#include "engine/highprecision/EphemerisDataManifest.hpp"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace skygate::ephemeris {
namespace {

constexpr int kSupportedSchemaVersion = 1;
constexpr double kMaxExactJsonInteger = 9007199254740991.0;

[[nodiscard]] std::string_view trimAsciiWhitespace(std::string_view text) noexcept
{
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
        text.remove_prefix(1U);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
        text.remove_suffix(1U);
    }
    return text;
}

[[nodiscard]] bool parseInt(const std::string_view text, int& value) noexcept
{
    const std::string_view trimmed = trimAsciiWhitespace(text);
    if (trimmed.empty()) {
        return false;
    }

    const char* begin = trimmed.data();
    const char* end = begin + trimmed.size();
    const std::from_chars_result result = std::from_chars(begin, end, value);
    return result.ec == std::errc{} && result.ptr == end;
}

[[nodiscard]] std::optional<CivilDateTime> parseUtcDate(std::string_view text) noexcept
{
    text = trimAsciiWhitespace(text);
    const std::size_t yearSearchStart = text.starts_with("-") ? 1U : 0U;
    const std::size_t firstDash = text.find('-', yearSearchStart);
    const std::size_t secondDash =
        firstDash == std::string_view::npos ? std::string_view::npos : text.find('-', firstDash + 1U);
    if (firstDash == std::string_view::npos || secondDash == std::string_view::npos) {
        return std::nullopt;
    }

    int year = 0;
    int month = 0;
    int day = 0;
    if (!parseInt(text.substr(0U, firstDash), year)
        || !parseInt(text.substr(firstDash + 1U, secondDash - firstDash - 1U), month)
        || !parseInt(text.substr(secondDash + 1U), day)) {
        return std::nullopt;
    }

    CivilDateTime dateTime;
    dateTime.astronomicalYear = year;
    dateTime.month = month;
    dateTime.day = day;
    dateTime.timeScale = TimeScale::Utc;
    if (!CalendarTime::isValidCivilDateTime(dateTime)) {
        return std::nullopt;
    }

    return dateTime;
}

[[nodiscard]] std::optional<AstronomicalEpoch> parseUtcDateEpoch(const QString& text) noexcept
{
    const std::string value = text.toStdString();
    const std::optional<CivilDateTime> dateTime = parseUtcDate(value);
    if (!dateTime.has_value()) {
        return std::nullopt;
    }

    return CalendarTime::astronomicalEpochFromCivilDateTime(*dateTime);
}

[[nodiscard]] bool hasNonEmptyString(const QJsonObject& object, const QString& key) noexcept
{
    const QJsonValue value = object.value(key);
    return value.isString() && !value.toString().trimmed().isEmpty();
}

[[nodiscard]] std::string stringValue(const QJsonObject& object, const QString& key)
{
    return object.value(key).toString().trimmed().toStdString();
}

[[nodiscard]] bool readRequiredString(
    const QJsonObject& object,
    const QString& key,
    const std::string_view context,
    std::vector<std::string>& diagnostics,
    std::string& value
)
{
    if (!hasNonEmptyString(object, key)) {
        diagnostics.push_back(std::string(context) + " requires non-empty string field '" + key.toStdString() + "'.");
        return false;
    }

    value = stringValue(object, key);
    return true;
}

[[nodiscard]] bool readOptionalString(const QJsonObject& object, const QString& key, std::string& value)
{
    const QJsonValue jsonValue = object.value(key);
    if (!jsonValue.isString()) {
        return false;
    }

    value = jsonValue.toString().trimmed().toStdString();
    return !value.empty();
}

[[nodiscard]] bool readRequiredBool(
    const QJsonObject& object,
    const QString& key,
    const std::string_view context,
    std::vector<std::string>& diagnostics,
    bool& value
)
{
    const QJsonValue jsonValue = object.value(key);
    if (!jsonValue.isBool()) {
        diagnostics.push_back(std::string(context) + " field '" + key.toStdString() + "' must be a boolean.");
        return false;
    }

    value = jsonValue.toBool();
    return true;
}

[[nodiscard]] bool readOptionalBool(
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
        diagnostics.push_back(std::string(context) + " field '" + key.toStdString() + "' must be a boolean.");
        return false;
    }

    value = jsonValue.toBool();
    return true;
}

[[nodiscard]] bool parseAssetKind(const std::string_view text, EphemerisDataManifestAssetKind& kind) noexcept
{
    if (text == "solar-system-kernel") {
        kind = EphemerisDataManifestAssetKind::SolarSystemKernel;
        return true;
    }
    if (text == "leap-second-table") {
        kind = EphemerisDataManifestAssetKind::LeapSecondTable;
        return true;
    }
    if (text == "earth-orientation-data") {
        kind = EphemerisDataManifestAssetKind::EarthOrientationData;
        return true;
    }
    if (text == "delta-t-data") {
        kind = EphemerisDataManifestAssetKind::DeltaTData;
        return true;
    }

    return false;
}

[[nodiscard]] bool
parseCompressionKind(const std::string_view text, EphemerisDataManifestCompressionKind& kind) noexcept
{
    if (text == "none") {
        kind = EphemerisDataManifestCompressionKind::None;
        return true;
    }
    if (text == "zstd") {
        kind = EphemerisDataManifestCompressionKind::Zstd;
        return true;
    }

    return false;
}

[[nodiscard]] bool readUInt64(
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
        diagnostics.push_back(std::string(context) + " field '" + key.toStdString() + "' must be an integer.");
        return false;
    }

    const double number = jsonValue.toDouble();
    if (!std::isfinite(number) || number < 0.0 || std::floor(number) != number) {
        diagnostics.push_back(
            std::string(context) + " field '" + key.toStdString() + "' must be a non-negative integer."
        );
        return false;
    }
    if (number > static_cast<double>(std::numeric_limits<std::uint64_t>::max())) {
        diagnostics.push_back(std::string(context) + " field '" + key.toStdString() + "' exceeds the uint64 range.");
        return false;
    }
    if (number > kMaxExactJsonInteger) {
        diagnostics.push_back(
            std::string(context) + " field '" + key.toStdString() + "' exceeds the exact JSON integer range."
        );
        return false;
    }

    value = static_cast<std::uint64_t>(number);
    return true;
}

[[nodiscard]] bool parseDateRange(
    const QJsonObject& object,
    const std::string_view context,
    std::vector<std::string>& diagnostics,
    EphemerisDateRange& range
)
{
    bool valid = true;
    valid &= readRequiredString(object, "id", context, diagnostics, range.id);
    valid &= readRequiredString(object, "displayName", context, diagnostics, range.displayName);

    if (!hasNonEmptyString(object, "start") || !hasNonEmptyString(object, "end")) {
        diagnostics.push_back(std::string(context) + " requires non-empty 'start' and 'end' UTC date fields.");
        return false;
    }

    const std::optional<AstronomicalEpoch> start = parseUtcDateEpoch(object.value("start").toString());
    const std::optional<AstronomicalEpoch> end = parseUtcDateEpoch(object.value("end").toString());
    if (!start.has_value() || !end.has_value()) {
        diagnostics.push_back(std::string(context) + " contains malformed UTC date fields.");
        return false;
    }
    if (!isFiniteEpoch(*start) || !isFiniteEpoch(*end) || epochSortKey(*start) > epochSortKey(*end)) {
        diagnostics.push_back(std::string(context) + " must have an ordered finite UTC validity range.");
        return false;
    }

    range.start = *start;
    range.end = *end;
    return valid;
}

[[nodiscard]] bool parseChecksum(
    const QJsonValue& value,
    const std::string_view context,
    std::vector<std::string>& diagnostics,
    EphemerisDataManifestChecksum& checksum
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

[[nodiscard]] bool parseCompression(
    const QJsonValue& value,
    const std::string_view context,
    std::vector<std::string>& diagnostics,
    EphemerisDataManifestCompression& compression
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
    if (!parseCompressionKind(format, compression.kind)) {
        diagnostics.push_back(std::string(context) + " has unsupported compression format '" + format + "'.");
        return false;
    }

    bool valid = true;
    valid &= readUInt64(object, "compressedSizeBytes", context, diagnostics, compression.compressedSizeBytes);
    valid &= readUInt64(object, "uncompressedSizeBytes", context, diagnostics, compression.uncompressedSizeBytes);
    if (compression.kind == EphemerisDataManifestCompressionKind::Zstd
        && (!compression.compressedSizeBytes.has_value() || !compression.uncompressedSizeBytes.has_value()
            || *compression.compressedSizeBytes == 0U || *compression.uncompressedSizeBytes == 0U)) {
        diagnostics.push_back(
            std::string(context) + " zstd compression requires positive compressed and uncompressed sizes."
        );
        valid = false;
    }

    return valid;
}

[[nodiscard]] bool
parseProfile(const QJsonValue& value, std::vector<std::string>& diagnostics, EphemerisDataManifestProfile& profile)
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
            std::string assetId = assetIdValue.toString().trimmed().toStdString();
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

[[nodiscard]] bool
parseAsset(const QJsonValue& value, std::vector<std::string>& diagnostics, EphemerisDataManifestAsset& asset)
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
        if (!parseAssetKind(kind, asset.kind)) {
            diagnostics.push_back("Asset has unsupported kind '" + kind + "'.");
            valid = false;
        }
    } else {
        valid = false;
    }

    valid &= readRequiredString(object, "profileId", context, diagnostics, asset.profileId);
    valid &= readRequiredString(object, "version", context, diagnostics, asset.version);
    valid &= readRequiredString(object, "sourceUrl", context, diagnostics, asset.sourceUrl);
    (void)readOptionalString(object, "relativePath", asset.relativePath);
    valid &= readOptionalBool(object, "optional", context, diagnostics, asset.optional);

    valid &= parseChecksum(object.value("checksum"), context, diagnostics, asset.checksum);
    valid &= parseCompression(object.value("compression"), context, diagnostics, asset.compression);
    if (!object.value("validityRange").isObject()) {
        diagnostics.push_back("Asset requires validityRange metadata.");
        valid = false;
    } else {
        valid &= parseDateRange(object.value("validityRange").toObject(), context, diagnostics, asset.validityRange);
    }

    return valid;
}

void validateProfileAssetReferences(const EphemerisDataManifest& manifest, std::vector<std::string>& diagnostics)
{
    std::unordered_set<std::string> profileIds;
    for (const EphemerisDataManifestProfile& profile : manifest.profiles) {
        if (!profileIds.insert(profile.id).second) {
            diagnostics.push_back("Manifest contains duplicate profile id '" + profile.id + "'.");
        }
    }

    std::unordered_set<std::string> assetIds;
    for (const EphemerisDataManifestAsset& asset : manifest.assets) {
        if (!assetIds.insert(asset.id).second) {
            diagnostics.push_back("Manifest contains duplicate asset id '" + asset.id + "'.");
        }
        if (manifest.profile(asset.profileId) == nullptr) {
            diagnostics.push_back("Asset '" + asset.id + "' references unknown profile '" + asset.profileId + "'.");
        }
    }

    for (const EphemerisDataManifestProfile& profile : manifest.profiles) {
        for (const std::string& assetId : profile.assetIds) {
            const EphemerisDataManifestAsset* asset = manifest.asset(assetId);
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

[[nodiscard]] bool parseDataSetInfo(
    const QJsonObject& rootObject, std::vector<std::string>& diagnostics, EphemerisDataSetInfo& dataSetInfo
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
            if (parseDateRange(dateRangeValue.toObject(), "Manifest date range", diagnostics, range)) {
                dataSetInfo.dateRanges.push_back(std::move(range));
            } else {
                valid = false;
            }
        }
    }

    return valid;
}

}  // namespace

const EphemerisDataManifestProfile* EphemerisDataManifest::profile(const std::string_view id) const noexcept
{
    const auto match =
        std::ranges::find_if(profiles, [id](const EphemerisDataManifestProfile& profile) { return profile.id == id; });
    return match == profiles.end() ? nullptr : &*match;
}

const EphemerisDataManifestAsset* EphemerisDataManifest::asset(const std::string_view id) const noexcept
{
    const auto match =
        std::ranges::find_if(assets, [id](const EphemerisDataManifestAsset& asset) { return asset.id == id; });
    return match == assets.end() ? nullptr : &*match;
}

EphemerisDataManifestParseResult parseEphemerisDataManifest(const std::string_view payload)
{
    EphemerisDataManifestParseResult result;
    if (trimAsciiWhitespace(payload).empty()) {
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
        result.diagnostics.push_back("Manifest schemaVersion must be 1.");
    }
    result.manifest.schemaVersion = kSupportedSchemaVersion;

    bool valid = parseDataSetInfo(rootObject, result.diagnostics, result.manifest.dataSetInfo);

    const QJsonValue profilesValue = rootObject.value("profiles");
    if (!profilesValue.isArray() || profilesValue.toArray().isEmpty()) {
        result.diagnostics.push_back("Manifest requires a non-empty profiles array.");
        valid = false;
    } else {
        for (const QJsonValue& profileValue : profilesValue.toArray()) {
            EphemerisDataManifestProfile profile;
            if (parseProfile(profileValue, result.diagnostics, profile)) {
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
            EphemerisDataManifestAsset asset;
            if (parseAsset(assetValue, result.diagnostics, asset)) {
                result.manifest.assets.push_back(std::move(asset));
            } else {
                valid = false;
            }
        }
    }

    const std::size_t diagnosticsBeforeReferenceValidation = result.diagnostics.size();
    validateProfileAssetReferences(result.manifest, result.diagnostics);
    valid = valid && diagnosticsBeforeReferenceValidation == result.diagnostics.size();

    if (valid && result.diagnostics.empty()) {
        result.status = EphemerisDataManifestStatus::Valid;
    }

    return result;
}

}  // namespace skygate::ephemeris
