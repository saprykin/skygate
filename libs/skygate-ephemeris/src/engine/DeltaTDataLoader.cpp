#include "DeltaTDataLoader.hpp"
#include "DeltaTDataParser.hpp"
#include "EphemerisTableLoading.hpp"
#include "IEphemerisDataSnapshot.hpp"
#include "TableBackedDeltaTProvider.hpp"

#include <optional>
#include <utility>
#include <vector>

namespace skygate::ephemeris {

namespace {

using LoadedDeltaTTable = EphemerisTableLoading::LoadedTable<IDeltaTProvider, IDeltaTProvider::DataInfo>;

[[nodiscard]] DeltaTDataLoader::Result toLoaderResult(LoadedDeltaTTable loaded)
{
    DeltaTDataLoader::Result result;
    result.dataInfo = std::move(loaded.info);
    result.provider = std::move(loaded.provider);
    return result;
}

}  // namespace

DeltaTDataLoader::Result
DeltaTDataLoader::loadFromSnapshot(const IEphemerisDataSnapshot& snapshot, const Options& options)
{
    const std::optional<EphemerisTextDataAsset> asset = snapshot.deltaTDataAsset();
    if (!asset.has_value()) {
        return toLoaderResult(
            EphemerisTableLoading::missingAsset<IDeltaTProvider, IDeltaTProvider::DataInfo>(
                "Delta T data asset is missing from the data snapshot."
            )
        );
    }

    return loadFromTextAsset(*asset, options);
}

DeltaTDataLoader::Result
DeltaTDataLoader::loadFromTextAsset(const EphemerisTextDataAsset& asset, const Options& options)
{
    DeltaTDataParser::Result parsed = DeltaTDataParser::parse(asset);
    const bool parseSucceeded = parsed.isSuccess();
    return toLoaderResult(
        EphemerisTableLoading::loadParsedTable<IDeltaTProvider, IDeltaTProvider::DataInfo>(
            std::move(parsed.dataInfo),
            std::move(parsed.entries),
            parseSucceeded,
            options.referenceEpoch,
            "Delta T data is stale for the reference epoch.",
            [](IDeltaTProvider::DataInfo info, std::vector<IDeltaTProvider::TableEntry> entries) {
                return std::make_shared<TableBackedDeltaTProvider>(std::move(info), std::move(entries));
            }
        )
    );
}

}  // namespace skygate::ephemeris
