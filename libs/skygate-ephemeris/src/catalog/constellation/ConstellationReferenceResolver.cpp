#include "ConstellationReferenceResolver.hpp"

#include "catalog/CatalogIdentifier.hpp"
#include "catalog/stellarium/StellariumHipParser.hpp"

#include <QLoggingCategory>
#include <QString>
#include <QStringList>

#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace skygate::ephemeris {
namespace {

Q_LOGGING_CATEGORY(skygateCatalogConstellationLog, "skygate.catalog.constellation")

constexpr std::size_t kMaxDiagnosticExamples = 5U;

// The Stellarium adapters emit constellation references using the canonical
// "hip_<n>" spelling. Interpretation of that spelling stays in the adapter's
// HIP parser; the resolver only delegates to it and then queries the identity
// index, so consumers never parse or normalize the HIP reference themselves.
std::optional<CatalogIdentifier> parseHipReference(const std::string_view reference)
{
    const std::optional<int> hipNumber = StellariumHipParser::parseStrictHipText(
        QString::fromUtf8(reference.data(), static_cast<qsizetype>(reference.size()))
    );
    if (!hipNumber.has_value()) {
        return std::nullopt;
    }

    return CatalogIdentifier::make("hip", std::to_string(*hipNumber));
}

struct ResolutionSummary final {
    std::size_t unresolvedCount = 0U;
    std::size_t ambiguousCount = 0U;
    std::vector<std::string> unresolvedExamples;
    std::vector<std::string> ambiguousExamples;
};

void recordResolution(ResolutionSummary& summary, const ConstellationReferenceResolver::Resolution& resolution)
{
    if (resolution.status == ConstellationReferenceResolver::Status::Unresolved) {
        ++summary.unresolvedCount;
        if (summary.unresolvedExamples.size() < kMaxDiagnosticExamples) {
            summary.unresolvedExamples.push_back(resolution.reference);
        }
        return;
    }

    if (resolution.status == ConstellationReferenceResolver::Status::Ambiguous) {
        ++summary.ambiguousCount;
        if (summary.ambiguousExamples.size() < kMaxDiagnosticExamples) {
            summary.ambiguousExamples.push_back(resolution.reference);
        }
    }
}

QString joinedExamples(const std::vector<std::string>& examples)
{
    QStringList texts;
    texts.reserve(static_cast<qsizetype>(examples.size()));
    for (const std::string& example : examples) {
        texts.append(QString::fromStdString(example));
    }
    return texts.join(QStringLiteral(", "));
}

void logResolutionSummary(const ResolutionSummary& summary)
{
    if (summary.unresolvedCount == 0U && summary.ambiguousCount == 0U) {
        return;
    }

    QStringList parts;
    if (summary.unresolvedCount > 0U) {
        QString part =
            QStringLiteral("%1 unresolved reference(s)").arg(static_cast<qulonglong>(summary.unresolvedCount));
        if (!summary.unresolvedExamples.empty()) {
            part += QStringLiteral(" (e.g. %1)").arg(joinedExamples(summary.unresolvedExamples));
        }
        parts.append(std::move(part));
    }
    if (summary.ambiguousCount > 0U) {
        QString part = QStringLiteral("%1 ambiguous reference(s)").arg(static_cast<qulonglong>(summary.ambiguousCount));
        if (!summary.ambiguousExamples.empty()) {
            part += QStringLiteral(" (e.g. %1)").arg(joinedExamples(summary.ambiguousExamples));
        }
        parts.append(std::move(part));
    }

    qCWarning(skygateCatalogConstellationLog).noquote()
        << "Constellation reference resolution skipped anchors:" << parts.join(QStringLiteral("; "));
}

}  // namespace

ConstellationReferenceResolver::ConstellationReferenceResolver(const std::span<const BaseCelestialBody* const> bodies)
    : m_bodies(bodies)
{
    m_index.reserve(m_bodies.size());
    for (std::size_t index = 0; index < m_bodies.size(); ++index) {
        const BaseCelestialBody* body = m_bodies[index];
        // Constellation references are HIP star identifiers, so only star
        // bodies can be anchors. Indexing only stars keeps the per-revision
        // identity index focused without changing resolution results.
        if (body != nullptr && body->kind == BaseCelestialBody::Kind::Star) {
            m_index.add(*body, index);
        }
    }
}

ConstellationReferenceResolver::Resolution
ConstellationReferenceResolver::resolve(const std::string_view reference) const
{
    Resolution result;
    result.reference = std::string(reference);

    const std::optional<CatalogIdentifier> identifier = parseHipReference(reference);
    if (!identifier.has_value()) {
        return result;
    }

    BaseCelestialBody synthetic;
    synthetic.id = std::string(reference);
    synthetic.kind = BaseCelestialBody::Kind::Star;
    synthetic.identity.externalIdentifiers.push_back(*identifier);

    const CatalogIdentityIndex::Resolution resolution = m_index.resolve(synthetic);
    if (resolution.hasSingleMatch() && resolution.index < m_bodies.size() && m_bodies[resolution.index] != nullptr) {
        result.status = Status::Resolved;
        result.bodyId = m_bodies[resolution.index]->id;
        return result;
    }

    if (resolution.isAmbiguous()) {
        result.status = Status::Ambiguous;
        return result;
    }

    return result;
}

std::vector<ConstellationLineRef>
ConstellationReferenceResolver::resolveLines(const std::span<const ConstellationLineRef> lineRefs) const
{
    std::vector<ConstellationLineRef> resolved;
    resolved.reserve(lineRefs.size());
    ResolutionSummary summary;
    for (const ConstellationLineRef& lineRef : lineRefs) {
        const Resolution start = resolve(lineRef.first);
        const Resolution end = resolve(lineRef.second);
        recordResolution(summary, start);
        recordResolution(summary, end);
        if (start.status != Status::Resolved || end.status != Status::Resolved) {
            continue;
        }

        resolved.emplace_back(start.bodyId, end.bodyId);
    }

    logResolutionSummary(summary);
    return resolved;
}

std::vector<ConstellationAnchorGroup>
ConstellationReferenceResolver::resolveAnchors(const std::span<const ConstellationAnchorGroup> anchorGroups) const
{
    std::vector<ConstellationAnchorGroup> resolved;
    resolved.reserve(anchorGroups.size());
    ResolutionSummary summary;
    for (const ConstellationAnchorGroup& anchorGroup : anchorGroups) {
        if (anchorGroup.first.empty()) {
            continue;
        }

        std::vector<std::string> resolvedIds;
        resolvedIds.reserve(anchorGroup.second.size());
        std::unordered_set<std::string> seenIds;
        seenIds.reserve(anchorGroup.second.size());
        for (const std::string& reference : anchorGroup.second) {
            const Resolution resolution = resolve(reference);
            recordResolution(summary, resolution);
            if (resolution.status != Status::Resolved) {
                continue;
            }
            if (seenIds.insert(resolution.bodyId).second) {
                resolvedIds.push_back(resolution.bodyId);
            }
        }

        if (!resolvedIds.empty()) {
            resolved.emplace_back(anchorGroup.first, std::move(resolvedIds));
        }
    }

    logResolutionSummary(summary);
    return resolved;
}

}  // namespace skygate::ephemeris
