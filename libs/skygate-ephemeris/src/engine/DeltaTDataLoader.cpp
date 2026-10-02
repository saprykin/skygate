#include "DeltaTDataLoader.hpp"
#include "DeltaTDataParser.hpp"
#include "TableBackedDeltaTProvider.hpp"

#include <utility>

namespace skygate::ephemeris {

DeltaTDataLoader::Result
DeltaTDataLoader::loadFromSnapshot(const IEphemerisDataSnapshot& snapshot, const Options& options)
{
    const std::optional<EphemerisTextDataAsset> asset = snapshot.deltaTDataAsset();
    if (!asset.has_value()) {
        Result result;
        result.dataInfo.status = IDeltaTProvider::DataStatus::Missing;
        result.dataInfo.diagnosticText = "Delta T data asset is missing from the data snapshot.";
        return result;
    }

    return loadFromTextAsset(*asset, options);
}

DeltaTDataLoader::Result
DeltaTDataLoader::loadFromTextAsset(const EphemerisTextDataAsset& asset, const Options& options)
{
    DeltaTDataParser::Result parsed = DeltaTDataParser::parse(asset);
    Result result;
    if (!parsed.isSuccess()) {
        result.dataInfo = std::move(parsed.dataInfo);
        return result;
    }

    auto& info = parsed.dataInfo;
    if (options.referenceEpoch.has_value() && info.expiresAt.has_value() && options.referenceEpoch->isFinite()
        && options.referenceEpoch->sortKey() > info.expiresAt->sortKey()) {
        info.status = IDeltaTProvider::DataStatus::Stale;
        info.diagnosticText = "Delta T data is stale for the reference epoch.";
    }

    result.dataInfo = info;
    result.provider = std::make_shared<TableBackedDeltaTProvider>(std::move(info), std::move(parsed.entries));
    return result;
}

}  // namespace skygate::ephemeris
