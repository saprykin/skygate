#include "OpenNgcObjectMapper.hpp"
#include "StringUtilities.hpp"
#include "catalog/CatalogIdentifier.hpp"
#include "catalog/io/CatalogParsingUtilities.hpp"

#include <QRegularExpression>
#include <QStringList>

#include <algorithm>
#include <optional>

namespace skygate::ephemeris {
namespace {

struct RecognizedDesignation final {
    QString identifierNamespace;
    QString value;
};

// Recognizes a primary NGC or IC designation, including supported component
// suffixes ("NGC0001", "ngc 1", "NGC1234A"). The value drops the catalog
// prefix and the zero padding but keeps the suffix; CatalogIdentifier applies
// the remaining per-namespace normalization.
std::optional<RecognizedDesignation> recognizedDesignation(const QString& text)
{
    static const QRegularExpression designationPattern(
        QStringLiteral("^(NGC|IC)\\s*(\\d+)([A-Za-z]*)$"), QRegularExpression::CaseInsensitiveOption
    );

    const QRegularExpressionMatch match = designationPattern.match(text.trimmed());
    if (!match.hasMatch()) {
        return std::nullopt;
    }

    return RecognizedDesignation{
        .identifierNamespace = match.captured(1).toLower(),
        .value = OpenNgcObjectMapper::withoutLeadingZeros(match.captured(2)) + match.captured(3).toLower(),
    };
}

QString normalizedCatalogAlias(QString text)
{
    text = text.trimmed();
    if (text.isEmpty()) {
        return {};
    }

    text.replace('_', ' ');
    text.replace('-', ' ');
    text = text.simplified();

    const auto normalizePrefix = [&text](const QString& prefix) {
        if (!text.startsWith(prefix, Qt::CaseInsensitive)) {
            return;
        }

        QString suffix = text.sliced(prefix.size()).trimmed();
        suffix = OpenNgcObjectMapper::withoutLeadingZeros(suffix);
        if (!suffix.isEmpty()) {
            text = QString("%1 %2").arg(prefix, suffix);
        }
    };

    normalizePrefix("NGC");
    normalizePrefix("IC");
    normalizePrefix("M");
    return text;
}

QString objectIdFromAlias(const QString& alias)
{
    const QString normalized = normalizedCatalogAlias(alias);
    if (normalized.startsWith("NGC ", Qt::CaseInsensitive)) {
        return "ngc_" + normalized.sliced(4).trimmed();
    }
    if (normalized.startsWith("IC ", Qt::CaseInsensitive)) {
        return "ic_" + normalized.sliced(3).trimmed();
    }
    if (normalized.startsWith("M ", Qt::CaseInsensitive)) {
        return "messier_" + normalized.sliced(2).trimmed().rightJustified(3, '0');
    }

    QString id = normalized.toLower();
    id.replace(QRegularExpression("[^a-z0-9]+"), "_");
    while (id.startsWith('_')) {
        id.remove(0, 1);
    }
    while (id.endsWith('_')) {
        id.chop(1);
    }
    return id.isEmpty() ? QString{} : "open_ngc_" + id;
}

void appendAlias(std::vector<std::string>& aliases, const QString& alias)
{
    const QString normalized = normalizedCatalogAlias(alias);
    if (normalized.isEmpty()) {
        return;
    }

    StringUtilities::appendUniqueIgnoreAsciiCase(aliases, CatalogParsingUtilities::toUtf8String(normalized));
}

void appendIdentifierUnique(std::vector<CatalogIdentifier>& identifiers, const CatalogIdentifier& identifier)
{
    if (identifier.empty()) {
        return;
    }
    if (std::find(identifiers.begin(), identifiers.end(), identifier) == identifiers.end()) {
        identifiers.push_back(identifier);
    }
}

void appendDelimitedAliases(std::vector<std::string>& aliases, const QString& text)
{
    for (const QString& token : text.split(',', Qt::SkipEmptyParts)) {
        appendAlias(aliases, token);
    }
}

DeepSkyObjectInfo::Kind kindFromOpenNgcType(QString typeText)
{
    typeText = typeText.trimmed();
    if (typeText == "G" || typeText == "GGroup" || typeText == "GPair" || typeText == "GTrpl") {
        return DeepSkyObjectInfo::Kind::Galaxy;
    }
    if (typeText == "OCl" || typeText == "Cl+N") {
        return DeepSkyObjectInfo::Kind::OpenCluster;
    }
    if (typeText == "GCl") {
        return DeepSkyObjectInfo::Kind::GlobularCluster;
    }
    if (typeText == "PN") {
        return DeepSkyObjectInfo::Kind::PlanetaryNebula;
    }
    if (typeText == "Neb" || typeText == "HII" || typeText == "EmN" || typeText == "RfN" || typeText == "SNR") {
        return DeepSkyObjectInfo::Kind::Nebula;
    }
    if (typeText == "*Ass") {
        return DeepSkyObjectInfo::Kind::Asterism;
    }
    return DeepSkyObjectInfo::Kind::Unknown;
}

}  // namespace

bool OpenNgcObjectMapper::shouldSkipType(const QString& typeText)
{
    return typeText == "NonEx" || typeText == "Dup" || typeText == "Star";
}

QString OpenNgcObjectMapper::withoutLeadingZeros(const QString& value)
{
    QString trimmed = value.trimmed();
    while (trimmed.size() > 1 && trimmed.startsWith('0')) {
        trimmed.remove(0, 1);
    }
    return trimmed;
}

OpenNgcObjectMapping OpenNgcObjectMapper::mapObject(
    const QString& typeText,
    const QString& name,
    const QString& messier,
    const QString& ngc,
    const QString& ic,
    const QString& identifiers,
    const QString& commonNames
)
{
    OpenNgcObjectMapping mapping;
    if (!messier.isEmpty()) {
        appendAlias(mapping.aliases, "M " + messier);
        mapping.externalIdentifiers.push_back(
            CatalogIdentifier::make("messier", CatalogParsingUtilities::toUtf8String(messier))
        );
    }
    if (!ngc.isEmpty()) {
        appendAlias(mapping.aliases, "NGC " + ngc);
        mapping.externalIdentifiers.push_back(
            CatalogIdentifier::make("ngc", CatalogParsingUtilities::toUtf8String(ngc))
        );
    }
    if (!ic.isEmpty()) {
        appendAlias(mapping.aliases, "IC " + ic);
        mapping.externalIdentifiers.push_back(CatalogIdentifier::make("ic", CatalogParsingUtilities::toUtf8String(ic)));
    }
    // The primary Name column is itself a designation column: a recognized
    // NGC/IC name is authoritative even when the optional cross-reference
    // columns are absent.
    if (const std::optional<RecognizedDesignation> designation = recognizedDesignation(name); designation.has_value()) {
        appendIdentifierUnique(
            mapping.externalIdentifiers,
            CatalogIdentifier::make(
                CatalogParsingUtilities::toUtf8String(designation->identifierNamespace),
                CatalogParsingUtilities::toUtf8String(designation->value)
            )
        );
    }
    mapping.sourceRecordId = CatalogParsingUtilities::toUtf8String(name.trimmed());
    appendAlias(mapping.aliases, name);
    appendDelimitedAliases(mapping.aliases, identifiers);
    appendDelimitedAliases(mapping.aliases, commonNames);

    const QString displayName =
        !messier.isEmpty()
            ? QString("M%1").arg(messier)
            : (!ngc.isEmpty() ? QString("NGC %1").arg(ngc)
                              : (!ic.isEmpty() ? QString("IC %1").arg(ic) : normalizedCatalogAlias(name)));
    const QString id =
        !messier.isEmpty()
            ? objectIdFromAlias("M " + messier)
            : (!ngc.isEmpty() ? objectIdFromAlias("NGC " + ngc)
                              : (!ic.isEmpty() ? objectIdFromAlias("IC " + ic) : objectIdFromAlias(name)));

    mapping.id = CatalogParsingUtilities::toUtf8String(id);
    mapping.displayName = CatalogParsingUtilities::toUtf8String(displayName);
    mapping.kind = kindFromOpenNgcType(typeText);
    return mapping;
}

}  // namespace skygate::ephemeris
