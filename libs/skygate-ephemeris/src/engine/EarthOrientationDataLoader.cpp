#include "EarthOrientationDataLoader.hpp"
#include "EarthOrientationDataParser.hpp"
#include "EphemerisTableLoading.hpp"
#include "IEphemerisDataSnapshot.hpp"
#include "TableBackedEarthOrientationProvider.hpp"

#include <optional>
#include <utility>
#include <vector>

namespace skygate::ephemeris {

EarthOrientationDataLoader::Result
EarthOrientationDataLoader::loadFromSnapshot(const IEphemerisDataSnapshot& snapshot, const Options& options)
{
    const std::optional<EphemerisTextDataAsset> asset = snapshot.earthOrientationDataAsset();
    if (!asset.has_value()) {
        return EphemerisTableLoading::toLoaderResult<Result>(
            EphemerisTableLoading::missingAsset<IEarthOrientationProvider, IEarthOrientationProvider::DataInfo>(
                "Earth-orientation data asset is missing from the data snapshot."
            ),
            &Result::dataInfo
        );
    }

    return loadFromTextAsset(*asset, options);
}

EarthOrientationDataLoader::Result
EarthOrientationDataLoader::loadFromTextAsset(const EphemerisTextDataAsset& asset, const Options& options)
{
    EarthOrientationDataParser::Result parsed = EarthOrientationDataParser::parse(asset);
    const bool parseSucceeded = parsed.isSuccess();
    return EphemerisTableLoading::toLoaderResult<Result>(
        EphemerisTableLoading::loadParsedTable<IEarthOrientationProvider, IEarthOrientationProvider::DataInfo>(
            std::move(parsed.dataInfo),
            std::move(parsed.entries),
            parseSucceeded,
            options.referenceEpoch,
            "Earth-orientation data is stale for the reference epoch.",
            [](IEarthOrientationProvider::DataInfo info, std::vector<IEarthOrientationProvider::TableEntry> entries) {
                return std::make_shared<TableBackedEarthOrientationProvider>(std::move(info), std::move(entries));
            }
        ),
        &Result::dataInfo
    );
}

}  // namespace skygate::ephemeris
