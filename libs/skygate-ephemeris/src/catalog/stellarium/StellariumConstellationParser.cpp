#include "StellariumConstellationParser.hpp"
#include "StellariumAnchorGroupExtractor.hpp"
#include "StellariumHipParser.hpp"
#include "StellariumLineRefExtractor.hpp"
#include "StringUtilities.hpp"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace skygate::ephemeris {
namespace {

// True when the payload's first non-whitespace byte starts a JSON object or
// array. A whitespace-only payload does not look like JSON. Whitespace is the
// shared ASCII predicate, so JSON framing accepts the same leading bytes every
// other payload probe does.
[[nodiscard]] bool looksLikeJsonPayload(const std::string_view payload) noexcept
{
    for (const char byte : payload) {
        if (StringUtilities::isAsciiWhitespace(byte)) {
            continue;
        }
        return byte == '{' || byte == '[';
    }

    return false;
}

}  // namespace

StellariumConstellationParser::ParseResult StellariumConstellationParser::parse(const std::string_view payload) const
{
    ParseResult result;
    if (payload.empty()) {
        return result;
    }

    if (!looksLikeJsonPayload(payload)) {
        return result;
    }

    QJsonParseError parseError;
    const QByteArray payloadBytes(payload.data(), static_cast<qsizetype>(payload.size()));
    const QJsonDocument document = QJsonDocument::fromJson(payloadBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return result;
    }

    const QJsonObject rootObject = document.object();
    result.lineRefs = StellariumLineRefExtractor::extract(rootObject);
    result.anchorGroups = StellariumAnchorGroupExtractor::extract(rootObject);
    result.constellationCount = StellariumHipParser::constellationArrayEntryCount(rootObject);
    return result;
}

}  // namespace skygate::ephemeris
