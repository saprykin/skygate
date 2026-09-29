#include "EarthOrientationDataLoader.hpp"
#include "EarthOrientationDataParser.hpp"
#include "EphemerisDataSnapshot.hpp"
#include "TableBackedEarthOrientationProvider.hpp"

#include <utility>

namespace skygate::ephemeris {

EarthOrientationDataLoader::Result
EarthOrientationDataLoader::loadFromSnapshot(const IEphemerisDataSnapshot& snapshot, const Options& options)
{
    const std::optional<EphemerisTextDataAsset> asset = snapshot.earthOrientationDataAsset();
    if (!asset.has_value()) {
        Result result;
        result.dataInfo.status = IEarthOrientationProvider::DataStatus::Missing;
        result.dataInfo.diagnosticText = "Earth-orientation data asset is missing from the data snapshot.";
        return result;
    }
    return loadFromTextAsset(*asset, options);
}

EarthOrientationDataLoader::Result
EarthOrientationDataLoader::loadFromTextAsset(const EphemerisTextDataAsset& asset, const Options& options)
{
    EarthOrientationDataParser::Result parsed = EarthOrientationDataParser::parse(asset);
    Result result;
    if (!parsed.isSuccess()) {
        result.dataInfo = std::move(parsed.dataInfo);
        return result;
    }
    auto& info = parsed.dataInfo;
    if (options.referenceEpoch.has_value() && info.expiresAt.has_value() && options.referenceEpoch->isFiniteUtc()
        && options.referenceEpoch->sortKey() > info.expiresAt->sortKey()) {
        info.status = IEarthOrientationProvider::DataStatus::Stale;
        info.diagnosticText = "Earth-orientation data is stale for the reference epoch.";
    }
    result.dataInfo = info;
    result.provider = std::make_shared<TableBackedEarthOrientationProvider>(std::move(info), std::move(parsed.entries));
    return result;
}

}  // namespace skygate::ephemeris
