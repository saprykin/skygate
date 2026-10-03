#include "TimeScaleProviderLoader.hpp"
#include "DeltaTDataLoader.hpp"
#include "EarthOrientationDataLoader.hpp"
#include "LeapSecondTableLoader.hpp"
#include "LeapSecondTimeScaleService.hpp"
#include "TimeScaleServiceOptions.hpp"

#include <memory>

namespace skygate::ephemeris {

std::shared_ptr<const IEarthOrientationProvider>
TimeScaleProviderLoader::loadEarthOrientationProvider(const IEphemerisDataSnapshot& snapshot)
{
    const EarthOrientationDataLoader::Result result = EarthOrientationDataLoader::loadFromSnapshot(snapshot);
    return result.isSuccess() ? result.provider : nullptr;
}

std::shared_ptr<const ITimeScaleService> TimeScaleProviderLoader::loadTimeScaleService(
    const IEphemerisDataSnapshot& snapshot,
    const std::shared_ptr<const IEarthOrientationProvider>& earthOrientationProvider
)
{
    const LeapSecondTableLoader::Result leapSecondTable = LeapSecondTableLoader::loadFromSnapshot(snapshot);
    if (!leapSecondTable.isSuccess()) {
        return nullptr;
    }

    const DeltaTDataLoader::Result deltaTData = DeltaTDataLoader::loadFromSnapshot(snapshot);
    TimeScaleServiceOptions timeScaleOptions;
    timeScaleOptions.allowDegradedLeapSecondFallback = true;
    timeScaleOptions.allowUt1DeltaTFallback = true;
    timeScaleOptions.earthOrientationSampleOptions.allowOutOfRangeNearestSampleFallback = true;
    timeScaleOptions.earthOrientationSampleOptions.allowMissingDataZeroFallback = true;
    timeScaleOptions.earthOrientationSampleOptions.degradePredictedData = false;

    return std::make_shared<LeapSecondTimeScaleService>(
        leapSecondTable.provider,
        timeScaleOptions,
        earthOrientationProvider,
        deltaTData.isSuccess() ? deltaTData.provider : nullptr
    );
}

}  // namespace skygate::ephemeris
