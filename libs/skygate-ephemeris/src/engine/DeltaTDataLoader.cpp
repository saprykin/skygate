#include "DeltaTDataLoader.hpp"
#include "DeltaTDataParser.hpp"
#include "EphemerisTableLoading.hpp"
#include "IEphemerisDataSnapshot.hpp"
#include "TableBackedDeltaTProvider.hpp"

#include <optional>
#include <utility>
#include <vector>

namespace skygate::ephemeris {

DeltaTDataLoader::Result
DeltaTDataLoader::loadFromSnapshot(const IEphemerisDataSnapshot& snapshot, const Options& options)
{
    const std::optional<EphemerisTextDataAsset> asset = snapshot.deltaTDataAsset();
    if (!asset.has_value()) {
        return EphemerisTableLoading::toLoaderResult<Result>(
            EphemerisTableLoading::missingAsset<IDeltaTProvider, IDeltaTProvider::DataInfo>(
                "Delta T data asset is missing from the data snapshot."
            ),
            &Result::dataInfo
        );
    }

    return loadFromTextAsset(*asset, options);
}

DeltaTDataLoader::Result
DeltaTDataLoader::loadFromTextAsset(const EphemerisTextDataAsset& asset, const Options& options)
{
    DeltaTDataParser::Result parsed = DeltaTDataParser::parse(asset);
    const bool parseSucceeded = parsed.isSuccess();
    return EphemerisTableLoading::toLoaderResult<Result>(
        EphemerisTableLoading::loadParsedTable<IDeltaTProvider, IDeltaTProvider::DataInfo>(
            std::move(parsed.dataInfo),
            std::move(parsed.entries),
            parseSucceeded,
            options.referenceEpoch,
            "Delta T data is stale for the reference epoch.",
            [](IDeltaTProvider::DataInfo info, std::vector<IDeltaTProvider::TableEntry> entries) {
                return std::make_shared<TableBackedDeltaTProvider>(std::move(info), std::move(entries));
            }
        ),
        &Result::dataInfo
    );
}

}  // namespace skygate::ephemeris
