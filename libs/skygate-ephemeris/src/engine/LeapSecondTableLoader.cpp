#include "LeapSecondTableLoader.hpp"
#include "EphemerisTableLoading.hpp"
#include "IEphemerisDataSnapshot.hpp"
#include "LeapSecondTableParser.hpp"
#include "TableBackedLeapSecondProvider.hpp"

#include <optional>
#include <utility>
#include <vector>

namespace skygate::ephemeris {

LeapSecondTableLoader::Result LeapSecondTableLoader::loadFromSnapshot(
    const IEphemerisDataSnapshot& snapshot, const LeapSecondTableLoader::Options& options
)
{
    const std::optional<EphemerisTextDataAsset> asset = snapshot.leapSecondTableAsset();
    if (!asset.has_value()) {
        return EphemerisTableLoading::toLoaderResult<Result>(
            EphemerisTableLoading::missingAsset<ILeapSecondProvider, ILeapSecondProvider::TableInfo>(
                "Leap-second table asset is missing from the data snapshot."
            ),
            &Result::tableInfo
        );
    }

    return loadFromTextAsset(*asset, options);
}

LeapSecondTableLoader::Result LeapSecondTableLoader::loadFromTextAsset(
    const EphemerisTextDataAsset& asset, const LeapSecondTableLoader::Options& options
)
{
    LeapSecondTableParser::Result parsed = LeapSecondTableParser::parse(asset);
    const bool parseSucceeded = parsed.isSuccess();
    return EphemerisTableLoading::toLoaderResult<Result>(
        EphemerisTableLoading::loadParsedTable<ILeapSecondProvider, ILeapSecondProvider::TableInfo>(
            std::move(parsed.tableInfo),
            std::move(parsed.entries),
            parseSucceeded,
            options.referenceEpoch,
            "Leap-second table is stale for the reference epoch.",
            [](ILeapSecondProvider::TableInfo tableInfo, std::vector<ILeapSecondProvider::TableEntry> entries) {
                return std::make_shared<TableBackedLeapSecondProvider>(std::move(tableInfo), std::move(entries));
            }
        ),
        &Result::tableInfo
    );
}

}  // namespace skygate::ephemeris
