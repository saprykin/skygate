#include "EarthOrientationDataLoader.hpp"
#include "EarthOrientationDataParser.hpp"
#include "EphemerisTableLoading.hpp"
#include "IEphemerisDataSnapshot.hpp"
#include "TableBackedEarthOrientationProvider.hpp"

#include <optional>
#include <utility>
#include <vector>

namespace skygate::ephemeris {

namespace {

using LoadedEarthOrientationTable =
    EphemerisTableLoading::LoadedTable<IEarthOrientationProvider, IEarthOrientationProvider::DataInfo>;

[[nodiscard]] EarthOrientationDataLoader::Result toLoaderResult(LoadedEarthOrientationTable loaded)
{
    EarthOrientationDataLoader::Result result;
    result.dataInfo = std::move(loaded.info);
    result.provider = std::move(loaded.provider);
    return result;
}

}  // namespace

EarthOrientationDataLoader::Result
EarthOrientationDataLoader::loadFromSnapshot(const IEphemerisDataSnapshot& snapshot, const Options& options)
{
    const std::optional<EphemerisTextDataAsset> asset = snapshot.earthOrientationDataAsset();
    if (!asset.has_value()) {
        return toLoaderResult(
            EphemerisTableLoading::missingAsset<IEarthOrientationProvider, IEarthOrientationProvider::DataInfo>(
                "Earth-orientation data asset is missing from the data snapshot."
            )
        );
    }

    return loadFromTextAsset(*asset, options);
}

EarthOrientationDataLoader::Result
EarthOrientationDataLoader::loadFromTextAsset(const EphemerisTextDataAsset& asset, const Options& options)
{
    EarthOrientationDataParser::Result parsed = EarthOrientationDataParser::parse(asset);
    const bool parseSucceeded = parsed.isSuccess();
    return toLoaderResult(
        EphemerisTableLoading::loadParsedTable<IEarthOrientationProvider, IEarthOrientationProvider::DataInfo>(
            std::move(parsed.dataInfo),
            std::move(parsed.entries),
            parseSucceeded,
            options.referenceEpoch,
            "Earth-orientation data is stale for the reference epoch.",
            [](IEarthOrientationProvider::DataInfo info, std::vector<IEarthOrientationProvider::TableEntry> entries) {
                return std::make_shared<TableBackedEarthOrientationProvider>(std::move(info), std::move(entries));
            }
        )
    );
}

}  // namespace skygate::ephemeris
