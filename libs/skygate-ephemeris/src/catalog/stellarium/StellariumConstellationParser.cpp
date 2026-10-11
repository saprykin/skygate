#include "StellariumConstellationParser.hpp"
#include "StellariumAnchorGroupExtractor.hpp"
#include "StellariumHipParser.hpp"
#include "StellariumLineRefExtractor.hpp"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace skygate::ephemeris {
namespace {

bool looksLikeJsonPayload(const std::string_view payload)
{
    for (const char byte : payload) {
        if (byte == ' ' || byte == '\t' || byte == '\n' || byte == '\r') {
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
