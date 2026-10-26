#include "TimeScaleProviderLoader.hpp"
#include "DeltaTDataLoader.hpp"
#include "EarthOrientationDataLoader.hpp"
#include "EphemerisDataManifest.hpp"
#include "EphemerisLogging.hpp"
#include "LeapSecondTableLoader.hpp"
#include "LeapSecondTimeScaleService.hpp"
#include "TimeScaleServiceOptions.hpp"
#include "engine/highprecision/CalcephKernelProvider.hpp"

#include <QDebug>

#include <memory>
#include <utility>

namespace skygate::ephemeris {

namespace {

void logProviderFailure(const char* providerName, const std::string& diagnosticText)
{
    const QString detail = diagnosticText.empty() ? QStringLiteral("no diagnostic text was reported")
                                                  : QString::fromStdString(diagnosticText);
    qCWarning(skygateEphemerisEngineLog).noquote()
        << QStringLiteral("The ephemeris bootstrap could not compose the %1: %2")
               .arg(QString::fromLatin1(providerName), detail);
}

}  // namespace

TimeScaleProviderLoader::Result<IEarthOrientationProvider>
TimeScaleProviderLoader::loadEarthOrientationProvider(const IEphemerisDataSnapshot& snapshot)
{
    EarthOrientationDataLoader::Result loaded = EarthOrientationDataLoader::loadFromSnapshot(snapshot);
    if (loaded.isSuccess()) {
        return {
            .provider = std::move(loaded.provider),
            .status = Status::Loaded,
            .diagnosticText = {},
        };
    }

    const Status status = loaded.dataInfo.status == IEarthOrientationProvider::DataStatus::Missing
                              ? Status::MissingData
                              : Status::UnusableData;
    Result<IEarthOrientationProvider> result{
        .provider = nullptr,
        .status = status,
        .diagnosticText = std::move(loaded.dataInfo.diagnosticText),
    };
    logProviderFailure("Earth-orientation provider", result.diagnosticText);
    return result;
}

TimeScaleProviderLoader::Result<ITimeScaleService> TimeScaleProviderLoader::loadTimeScaleService(
    const IEphemerisDataSnapshot& snapshot,
    const std::shared_ptr<const IEarthOrientationProvider>& earthOrientationProvider
)
{
    LeapSecondTableLoader::Result leapSecondTable = LeapSecondTableLoader::loadFromSnapshot(snapshot);
    if (!leapSecondTable.isSuccess()) {
        const Status status = leapSecondTable.tableInfo.status == ILeapSecondProvider::TableStatus::Missing
                                  ? Status::MissingData
                                  : Status::UnusableData;
        Result<ITimeScaleService> result{
            .provider = nullptr,
            .status = status,
            .diagnosticText = std::move(leapSecondTable.tableInfo.diagnosticText),
        };
        logProviderFailure("time-scale service", result.diagnosticText);
        return result;
    }

    const DeltaTDataLoader::Result deltaTData = DeltaTDataLoader::loadFromSnapshot(snapshot);
    TimeScaleServiceOptions timeScaleOptions;
    timeScaleOptions.allowDegradedLeapSecondFallback = true;
    timeScaleOptions.allowUt1DeltaTFallback = true;
    timeScaleOptions.earthOrientationSampleOptions = EarthOrientationSampler::Options::tolerantFallback();

    return {
        .provider = std::make_shared<LeapSecondTimeScaleService>(
            leapSecondTable.provider,
            timeScaleOptions,
            earthOrientationProvider,
            deltaTData.isSuccess() ? deltaTData.provider : nullptr
        ),
        .status = Status::Loaded,
        .diagnosticText = {},
    };
}

TimeScaleProviderLoader::Providers TimeScaleProviderLoader::resolveMissing(
    const IEphemerisDataSnapshot& snapshot,
    const EphemerisDataManifest* dataManifest,
    const Providers& existing,
    const KernelOptions& kernelOptions
)
{
    Providers providers = existing;

    if (providers.earthOrientationProvider == nullptr) {
        providers.earthOrientationProvider = loadEarthOrientationProvider(snapshot).provider;
    }
    if (providers.timeScaleService == nullptr) {
        providers.timeScaleService = loadTimeScaleService(snapshot, providers.earthOrientationProvider).provider;
    }
    if (providers.calcephKernelProvider == nullptr && dataManifest != nullptr) {
        highprecision::CalcephKernelProvider::Options kernelProviderOptions;
        kernelProviderOptions.verifyChecksum = kernelOptions.verifyChecksum;
        providers.calcephKernelProvider = std::make_shared<highprecision::CalcephKernelProvider>(
            snapshot, *dataManifest, std::move(kernelProviderOptions), providers.timeScaleService
        );
    }

    return providers;
}

}  // namespace skygate::ephemeris
