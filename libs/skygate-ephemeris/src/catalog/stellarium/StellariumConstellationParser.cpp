#include "skygate/ephemeris/StellariumConstellationParser.hpp"

#include "catalog/stellarium/StellariumLabelRefExtractor.hpp"
#include "catalog/stellarium/StellariumLineRefExtractor.hpp"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

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

std::size_t inferConstellationCountFromJson(const QJsonObject& rootObject)
{
    const QJsonValue constellationsValue = rootObject.value("constellations");
    if (constellationsValue.isArray()) {
        return static_cast<std::size_t>(constellationsValue.toArray().size());
    }

    return 0;
}

}  // namespace

StellariumConstellationParser::ParseResult StellariumConstellationParser::parse(
    const std::string_view payload
) const
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
    result.labelRefs = StellariumLabelRefExtractor::extract(rootObject);
    result.constellationCount = inferConstellationCountFromJson(rootObject);
    return result;
}

}  // namespace skygate::ephemeris
