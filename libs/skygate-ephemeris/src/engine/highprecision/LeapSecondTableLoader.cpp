#include "LeapSecondTableLoader.hpp"
#include "LeapSecondTableParser.hpp"
#include "TableBackedLeapSecondProvider.hpp"

#include <utility>

namespace skygate::ephemeris {

LeapSecondTableLoader::Result LeapSecondTableLoader::loadFromSnapshot(
    const IEphemerisDataSnapshot& snapshot, const LeapSecondTableLoader::Options& options
)
{
    const std::optional<EphemerisTextDataAsset> asset = snapshot.leapSecondTableAsset();
    if (!asset.has_value()) {
        Result result;
        result.tableInfo.status = ILeapSecondProvider::TableStatus::Missing;
        result.tableInfo.diagnosticText = "Leap-second table asset is missing from the data snapshot.";
        return result;
    }

    return loadFromTextAsset(*asset, options);
}

LeapSecondTableLoader::Result LeapSecondTableLoader::loadFromTextAsset(
    const EphemerisTextDataAsset& asset, const LeapSecondTableLoader::Options& options
)
{
    LeapSecondTableParser::Result parsed = LeapSecondTableParser::parse(asset);
    Result result;
    if (!parsed.isSuccess()) {
        result.tableInfo = std::move(parsed.tableInfo);
        return result;
    }

    auto& info = parsed.tableInfo;
    if (options.referenceEpoch.has_value() && info.expiresAt.has_value() && options.referenceEpoch->isFiniteUtc()
        && options.referenceEpoch->sortKey() > info.expiresAt->sortKey()) {
        info.status = ILeapSecondProvider::TableStatus::Stale;
        info.diagnosticText = "Leap-second table is stale for the reference epoch.";
    }

    result.tableInfo = info;
    result.provider = std::make_shared<TableBackedLeapSecondProvider>(std::move(info), std::move(parsed.entries));
    return result;
}

}  // namespace skygate::ephemeris
