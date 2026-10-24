#include "LeapSecondTableLoader.hpp"
#include "EphemerisTableLoading.hpp"
#include "IEphemerisDataSnapshot.hpp"
#include "LeapSecondTableParser.hpp"
#include "TableBackedLeapSecondProvider.hpp"

#include <optional>
#include <utility>
#include <vector>

namespace skygate::ephemeris {

namespace {

using LoadedLeapSecondTable = EphemerisTableLoading::LoadedTable<ILeapSecondProvider, ILeapSecondProvider::TableInfo>;

[[nodiscard]] LeapSecondTableLoader::Result toLoaderResult(LoadedLeapSecondTable loaded)
{
    LeapSecondTableLoader::Result result;
    result.tableInfo = std::move(loaded.info);
    result.provider = std::move(loaded.provider);
    return result;
}

}  // namespace

LeapSecondTableLoader::Result LeapSecondTableLoader::loadFromSnapshot(
    const IEphemerisDataSnapshot& snapshot, const LeapSecondTableLoader::Options& options
)
{
    const std::optional<EphemerisTextDataAsset> asset = snapshot.leapSecondTableAsset();
    if (!asset.has_value()) {
        return toLoaderResult(
            EphemerisTableLoading::missingAsset<ILeapSecondProvider, ILeapSecondProvider::TableInfo>(
                "Leap-second table asset is missing from the data snapshot."
            )
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
    return toLoaderResult(
        EphemerisTableLoading::loadParsedTable<ILeapSecondProvider, ILeapSecondProvider::TableInfo>(
            std::move(parsed.tableInfo),
            std::move(parsed.entries),
            parseSucceeded,
            options.referenceEpoch,
            "Leap-second table is stale for the reference epoch.",
            [](ILeapSecondProvider::TableInfo tableInfo, std::vector<ILeapSecondProvider::TableEntry> entries) {
                return std::make_shared<TableBackedLeapSecondProvider>(std::move(tableInfo), std::move(entries));
            }
        )
    );
}

}  // namespace skygate::ephemeris
