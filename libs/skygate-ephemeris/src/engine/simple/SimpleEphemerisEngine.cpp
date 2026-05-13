#include "skygate/ephemeris/EphemerisEngineFactory.hpp"

#include "StringUtilities.hpp"
#include "engine/highprecision/CalcephKernelProvider.hpp"
#include "engine/highprecision/FrameTransformer.hpp"
#include "engine/highprecision/HighPrecisionEphemerisEngine.hpp"
#include "engine/highprecision/SolarSystemStateCalculator.hpp"
#include "engine/simple/EquatorialToHorizontalCalculator.hpp"
#include "engine/simple/MoonEquatorialCalculator.hpp"
#include "engine/simple/PlanetEquatorialCalculator.hpp"
#include "engine/simple/SunEquatorialCalculator.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace skygate::ephemeris {

namespace {

constexpr std::string_view kSimpleDataSourceProvenance = "Simple ephemeris engine";
constexpr std::string_view kSimpleEngineName = "Simple ephemeris engine";
constexpr std::string_view kSimpleDataSetId = "simple";
constexpr std::string_view kSimpleDataSetVersion = "built-in";
constexpr double kSecondsPerDay = 86'400.0;
constexpr double kUnixEpochJulianDay = 2'440'587.5;

[[nodiscard]] bool hasExplicitEpoch(const AstronomicalEpoch& epoch) noexcept
{
    return std::isfinite(epoch.julianDatePart1) && std::isfinite(epoch.julianDatePart2)
           && (epoch.julianDatePart1 != 0.0 || epoch.julianDatePart2 != 0.0);
}

[[nodiscard]] core::UtcTimePoint utcTimeFromEpoch(const AstronomicalEpoch& epoch) noexcept
{
    const double julianDay = epoch.julianDatePart1 + epoch.julianDatePart2;
    const double epochSeconds = std::round((julianDay - kUnixEpochJulianDay) * kSecondsPerDay);
    return core::UtcTimePoint(std::chrono::seconds(static_cast<std::int64_t>(epochSeconds)));
}

[[nodiscard]] AstronomicalEpoch epochFromUtcTime(const core::UtcTimePoint& utcTime) noexcept
{
    const double julianDay =
        static_cast<double>(utcTime.time_since_epoch().count()) / kSecondsPerDay + kUnixEpochJulianDay;
    const double julianDatePart1 = std::floor(julianDay);
    return AstronomicalEpoch{
        .julianDatePart1 = julianDatePart1,
        .julianDatePart2 = julianDay - julianDatePart1,
        .timeScale = TimeScale::Utc,
    };
}

[[nodiscard]] core::SkyContext contextFromRequest(const EphemerisRequest& request) noexcept
{
    core::SkyContext context = request.context;
    if (request.epoch.timeScale == TimeScale::Utc && hasExplicitEpoch(request.epoch)) {
        context.utcTime = utcTimeFromEpoch(request.epoch);
    }

    return context;
}

[[nodiscard]] bool requestsUnsupportedSimpleOptions(const EphemerisEngineOptions& options) noexcept
{
    return options.correctionFlags != EphemerisCorrectionFlags::NoCorrections || options.enableAtmosphericRefraction;
}

void markUnsupportedSimpleOptions(CelestialBodyState& state, const EphemerisEngineOptions& options) noexcept
{
    if (!requestsUnsupportedSimpleOptions(options)) {
        return;
    }

    if (state.metadata.status == EphemerisResultStatus::Valid) {
        state.metadata.status = EphemerisResultStatus::Degraded;
    }
    state.metadata.addWarning(EphemerisWarningCode::CorrectionUnavailable);
    state.metadata.appliedCorrections = EphemerisCorrectionFlags::NoCorrections;
}

void markUnsupportedSimpleOptions(SkySnapshot& snapshot, const EphemerisEngineOptions& options) noexcept
{
    if (!requestsUnsupportedSimpleOptions(options)) {
        return;
    }

    for (CelestialBodyState& state : snapshot.states) {
        markUnsupportedSimpleOptions(state, options);
    }
}

[[nodiscard]] EphemerisEngineOptions simpleEngineDefaultOptions() noexcept
{
    EphemerisEngineOptions engineOptions;
    engineOptions.engineKind = EphemerisEngineKind::Simple;
    engineOptions.correctionFlags = EphemerisCorrectionFlags::NoCorrections;
    engineOptions.enableAtmosphericRefraction = false;
    return engineOptions;
}

[[nodiscard]] EphemerisEngineOptions simpleEngineOptionsFromRequest(const EphemerisEngineOptions& requestOptions
) noexcept
{
    EphemerisEngineOptions engineOptions = requestOptions;
    engineOptions.engineKind = EphemerisEngineKind::Simple;
    return engineOptions;
}

[[nodiscard]] EphemerisFactoryCreationDiagnostic makeDiagnostic(
    const EphemerisFactoryCreationDiagnosticCode code,
    const EphemerisFactoryCreationDiagnosticSeverity severity,
    std::string text = {}
)
{
    return {code, severity, std::move(text)};
}

[[nodiscard]] EphemerisEngineFactoryResult makeInvalidFactoryRequestResult()
{
    return EphemerisEngineFactoryResult::failure(
        EphemerisFactoryCreationStatus::FailedInvalidRequest,
        {EphemerisFactoryCreationDiagnostic{
            EphemerisFactoryCreationDiagnosticCode::InvalidRequest,
            EphemerisFactoryCreationDiagnosticSeverity::Error,
            "The requested ephemeris engine kind is not supported.",
        }}
    );
}

[[nodiscard]] EphemerisFactoryCreationStatus
highPrecisionFailureStatus(const std::vector<EphemerisFactoryCreationDiagnostic>& diagnostics) noexcept
{
    for (const EphemerisFactoryCreationDiagnostic& diagnostic : diagnostics) {
        if (diagnostic.code == EphemerisFactoryCreationDiagnosticCode::EngineCreationFailed
            || diagnostic.code == EphemerisFactoryCreationDiagnosticCode::InvalidRequest) {
            return EphemerisFactoryCreationStatus::FailedCreationError;
        }
    }

    return EphemerisFactoryCreationStatus::FailedStrictHighPrecisionUnavailable;
}

[[nodiscard]] std::vector<EphemerisFactoryCreationDiagnostic> withSeverity(
    std::vector<EphemerisFactoryCreationDiagnostic> diagnostics,
    const EphemerisFactoryCreationDiagnosticSeverity severity
)
{
    for (EphemerisFactoryCreationDiagnostic& diagnostic : diagnostics) {
        diagnostic.severity = severity;
    }

    return diagnostics;
}

void appendCalcephDiagnostics(
    std::vector<EphemerisFactoryCreationDiagnostic>& diagnostics,
    const highprecision::CalcephKernelProvider& kernelProvider
)
{
    const EphemerisFactoryCreationDiagnosticCode code =
        kernelProvider.status() == highprecision::CalcephKernelProviderStatus::CalcephUnavailable
            ? EphemerisFactoryCreationDiagnosticCode::HighPrecisionUnavailable
            : EphemerisFactoryCreationDiagnosticCode::RequiredEphemerisDataUnavailable;

    if (kernelProvider.diagnostics().empty()) {
        diagnostics.push_back(makeDiagnostic(code, EphemerisFactoryCreationDiagnosticSeverity::Error));
        return;
    }

    for (const std::string& diagnosticText : kernelProvider.diagnostics()) {
        diagnostics.push_back(makeDiagnostic(code, EphemerisFactoryCreationDiagnosticSeverity::Error, diagnosticText));
    }
}

void appendKernelDateRangeIfMissing(
    EphemerisDataSetInfo& dataSetInfo, const highprecision::CalcephKernelProvider& kernelProvider
)
{
    const std::optional<highprecision::CalcephKernelInfo>& kernelInfo = kernelProvider.kernelInfo();
    if (!kernelInfo.has_value()) {
        return;
    }

    for (const EphemerisDateRange& range : dataSetInfo.dateRanges) {
        if (range.id == kernelInfo->validityRange.id) {
            return;
        }
    }

    dataSetInfo.dateRanges.push_back(kernelInfo->validityRange);
}

}  // namespace

class SimpleEphemerisEngine final : public IEphemerisEngine {
public:
    explicit SimpleEphemerisEngine(
        std::span<const CelestialBody> bodies, EphemerisEngineOptions engineOptions = simpleEngineDefaultOptions()
    )
        : m_bodies(std::make_shared<const std::vector<CelestialBody>>(bodies.begin(), bodies.end())),
          m_options(simpleEngineOptionsFromRequest(engineOptions))
    {
    }

    [[nodiscard]] EphemerisEngineKind kind() const noexcept override
    {
        return EphemerisEngineKind::Simple;
    }

    [[nodiscard]] std::string_view name() const noexcept override
    {
        return kSimpleEngineName;
    }

    [[nodiscard]] EphemerisCapabilities capabilities() const noexcept override
    {
        EphemerisCapabilities engineCapabilities;
        engineCapabilities.engineKind = EphemerisEngineKind::Simple;
        engineCapabilities.supportedCorrections = EphemerisCorrectionFlags::NoCorrections;
        engineCapabilities.supportsSolarSystemBodies = true;
        engineCapabilities.supportsCatalogStars = true;
        engineCapabilities.supportsTopocentricPositions = true;
        engineCapabilities.supportsAtmosphericRefraction = false;
        engineCapabilities.supportsExtendedHistoricalRange = false;
        return engineCapabilities;
    }

    [[nodiscard]] std::span<const EphemerisDateRange> supportedDateRanges() const noexcept override
    {
        return {};
    }

    [[nodiscard]] EphemerisDataSetInfo dataSetInfo() const override
    {
        EphemerisDataSetInfo info;
        info.id = kSimpleDataSetId;
        info.displayName = kSimpleEngineName;
        info.version = kSimpleDataSetVersion;
        info.provenance = kSimpleDataSourceProvenance;
        return info;
    }

    [[nodiscard]] EphemerisEngineOptions options() const noexcept override
    {
        return m_options;
    }

    [[nodiscard]] SkySnapshot compute(const EphemerisRequest& request) const override
    {
        SkySnapshot snapshot = computeSnapshot(contextFromRequest(request));
        markUnsupportedSimpleOptions(snapshot, request.options);
        return snapshot;
    }

    [[nodiscard]] std::optional<CelestialBodyState>
    computeBodyState(const EphemerisRequest& request, const std::string_view bodyId) const override
    {
        std::optional<CelestialBodyState> state = computeBodyStateById(contextFromRequest(request), bodyId);
        if (state.has_value()) {
            markUnsupportedSimpleOptions(*state, request.options);
        }
        return state;
    }

    [[nodiscard]] std::optional<CelestialBodyState>
    computeBodyState(const EphemerisRequest& request, const std::size_t bodyIndex) const override
    {
        if (bodyIndex >= m_bodies->size()) {
            return std::nullopt;
        }

        CelestialBodyState state = computeStateForBody((*m_bodies)[bodyIndex], bodyIndex, contextFromRequest(request));
        markUnsupportedSimpleOptions(state, request.options);
        return state;
    }

    [[nodiscard]] SkySnapshot compute(const core::SkyContext& context) const override
    {
        return compute(makeCompatibilityRequest(context));
    }

    [[nodiscard]] std::optional<CelestialBodyState>
    computeBodyState(const core::SkyContext& context, const std::string_view bodyId) const override
    {
        return computeBodyState(makeCompatibilityRequest(context), bodyId);
    }

    [[nodiscard]] std::optional<CelestialBodyState>
    computeBodyState(const core::SkyContext& context, const std::uint32_t bodyIndex) const override
    {
        return computeBodyState(makeCompatibilityRequest(context), static_cast<std::size_t>(bodyIndex));
    }

private:
    [[nodiscard]] EphemerisRequest makeCompatibilityRequest(const core::SkyContext& context) const noexcept
    {
        EphemerisRequest request;
        request.epoch = epochFromUtcTime(context.utcTime);
        request.context = context;
        request.options = options();
        return request;
    }

    [[nodiscard]] SkySnapshot computeSnapshot(const core::SkyContext& context) const
    {
        SkySnapshot snapshot;
        snapshot.context = context;
        snapshot.catalogBodies = m_bodies;

        snapshot.states.reserve(m_bodies->size());
        for (std::size_t bodyIndex = 0; bodyIndex < m_bodies->size(); ++bodyIndex) {
            const CelestialBody& body = (*m_bodies)[bodyIndex];
            snapshot.states.push_back(computeStateForBody(body, bodyIndex, context));
        }

        return snapshot;
    }

    [[nodiscard]] std::optional<CelestialBodyState>
    computeBodyStateById(const core::SkyContext& context, const std::string_view bodyId) const
    {
        if (bodyId.empty()) {
            return std::nullopt;
        }

        for (std::size_t bodyIndex = 0; bodyIndex < m_bodies->size(); ++bodyIndex) {
            const CelestialBody& body = (*m_bodies)[bodyIndex];
            if (strings::equalsIgnoreAsciiCase(body.id, bodyId)) {
                return computeStateForBody(body, bodyIndex, context);
            }
        }

        return std::nullopt;
    }
    [[nodiscard]] CelestialBodyState
    computeStateForBody(const CelestialBody& body, const std::size_t bodyIndex, const core::SkyContext& context) const
    {
        CelestialBodyState state;
        state.bodyIndex = static_cast<std::uint32_t>(bodyIndex);
        state.equatorial.rightAscensionHours = std::numeric_limits<double>::quiet_NaN();
        state.equatorial.declinationDeg = std::numeric_limits<double>::quiet_NaN();
        state.horizontal.altitudeDeg = std::numeric_limits<double>::quiet_NaN();
        state.horizontal.azimuthDeg = std::numeric_limits<double>::quiet_NaN();
        state.metadata.dataSourceProvenance = kSimpleDataSourceProvenance;

        if (const auto equatorial = computeEquatorial(body, context.utcTime); equatorial.has_value()) {
            state.equatorial = *equatorial;
            if (context.observer.isValid()) {
                state.horizontal =
                    EquatorialToHorizontalCalculator::compute(*equatorial, context.observer, context.utcTime);
            } else {
                state.metadata.status = EphemerisResultStatus::Degraded;
                state.metadata.addWarning(EphemerisWarningCode::MissingObserver);
            }
        } else {
            state.metadata.status = EphemerisResultStatus::Unsupported;
            state.metadata.addWarning(EphemerisWarningCode::UnsupportedBody);
        }

        return state;
    }

    [[nodiscard]] std::optional<core::EquatorialCoordinate>
    computeEquatorial(const CelestialBody& body, const core::UtcTimePoint& utcTime) const
    {
        if (body.fixedEquatorial.has_value()) {
            return body.fixedEquatorial;
        }

        switch (body.ephemerisSource) {
        case CelestialBodyEphemerisSource::FixedEquatorial:
            break;
        case CelestialBodyEphemerisSource::Sun:
            return m_sunCalculator.compute(utcTime);
        case CelestialBodyEphemerisSource::Moon:
            return m_moonCalculator.compute(utcTime);
        case CelestialBodyEphemerisSource::Planet:
            return m_planetCalculator.compute(body.id, utcTime);
        case CelestialBodyEphemerisSource::Star:
            break;
        case CelestialBodyEphemerisSource::Constellation:
            break;
        case CelestialBodyEphemerisSource::Unresolved:
            break;
        }

        return std::nullopt;
    }

    std::shared_ptr<const std::vector<CelestialBody>> m_bodies;
    EphemerisEngineOptions m_options;
    SunEquatorialCalculator m_sunCalculator;
    MoonEquatorialCalculator m_moonCalculator;
    PlanetEquatorialCalculator m_planetCalculator;
};

[[nodiscard]] EphemerisEngineFactoryResult
createHighPrecisionEphemerisEngine(const EphemerisEngineFactoryRequest& request)
{
    std::vector<EphemerisFactoryCreationDiagnostic> diagnostics;

    if (request.activeDataSnapshot == nullptr) {
        diagnostics.push_back(makeDiagnostic(
            EphemerisFactoryCreationDiagnosticCode::RequiredEphemerisDataUnavailable,
            EphemerisFactoryCreationDiagnosticSeverity::Error,
            "An active ephemeris data snapshot is required for high-precision engine creation."
        ));
    }
    if (request.dataManifest == nullptr) {
        diagnostics.push_back(makeDiagnostic(
            EphemerisFactoryCreationDiagnosticCode::RequiredEphemerisDataUnavailable,
            EphemerisFactoryCreationDiagnosticSeverity::Error,
            "An ephemeris data manifest is required for high-precision engine creation."
        ));
    }
    if (request.timeScaleService == nullptr) {
        diagnostics.push_back(makeDiagnostic(
            EphemerisFactoryCreationDiagnosticCode::RequiredTimeScaleServiceUnavailable,
            EphemerisFactoryCreationDiagnosticSeverity::Error,
            "A time-scale service is required for high-precision engine creation."
        ));
    }
    if (request.earthOrientationProvider == nullptr) {
        diagnostics.push_back(makeDiagnostic(
            EphemerisFactoryCreationDiagnosticCode::RequiredEarthOrientationProviderUnavailable,
            EphemerisFactoryCreationDiagnosticSeverity::Error,
            "An Earth-orientation provider is required for high-precision engine creation."
        ));
    }

    if (diagnostics.empty()) {
        auto kernelProvider = std::make_shared<highprecision::CalcephKernelProvider>(
            *request.activeDataSnapshot,
            *request.dataManifest,
            highprecision::CalcephKernelSelectionOptions{},
            request.calcephKernelRuntime
        );
        if (!kernelProvider->isReady()) {
            appendCalcephDiagnostics(diagnostics, *kernelProvider);
        } else {
            highprecision::HighPrecisionEphemerisEngineDependencies dependencies;
            dependencies.calcephKernelProvider = kernelProvider;
            dependencies.solarSystemStateCalculator =
                std::make_shared<highprecision::SolarSystemStateCalculator>(kernelProvider);
            dependencies.timeScaleService = request.timeScaleService;
            dependencies.earthOrientationProvider = request.earthOrientationProvider;
            dependencies.frameTransformer = std::make_shared<highprecision::ErfaFrameTransformer>(
                request.timeScaleService, request.earthOrientationProvider
            );
            dependencies.dataSetInfo = request.dataManifest->dataSetInfo;
            if (request.dataSetManifest != nullptr) {
                dependencies.dataSetInfo = *request.dataSetManifest;
            }
            appendKernelDateRangeIfMissing(dependencies.dataSetInfo, *kernelProvider);

            return EphemerisEngineFactoryResult::success(std::make_unique<highprecision::HighPrecisionEphemerisEngine>(
                request.catalogBodies, request.options, std::move(dependencies)
            ));
        }
    }

    if (allowsSimpleEngineFallback(request.fallbackPolicy)) {
        return EphemerisEngineFactoryResult::success(
            std::make_unique<SimpleEphemerisEngine>(request.catalogBodies, request.options),
            EphemerisFactoryCreationStatus::CreatedSimpleFallback,
            withSeverity(std::move(diagnostics), EphemerisFactoryCreationDiagnosticSeverity::Warning)
        );
    }

    return EphemerisEngineFactoryResult::failure(highPrecisionFailureStatus(diagnostics), std::move(diagnostics));
}

EphemerisEngineFactoryResult createEphemerisEngine(const EphemerisEngineFactoryRequest& request)
{
    switch (request.engineKind) {
    case EphemerisEngineKind::Simple:
        return EphemerisEngineFactoryResult::success(
            std::make_unique<SimpleEphemerisEngine>(request.catalogBodies, request.options)
        );
    case EphemerisEngineKind::HighPrecision:
        return createHighPrecisionEphemerisEngine(request);
    }

    return makeInvalidFactoryRequestResult();
}

std::unique_ptr<IEphemerisEngine> createEphemerisEngine()
{
    EphemerisEngineFactoryRequest request;
    request.options = simpleEngineDefaultOptions();
    EphemerisEngineFactoryResult result = createEphemerisEngine(request);
    return std::move(result.engine);
}

std::unique_ptr<IEphemerisEngine> createEphemerisEngine(const IStarCatalog& catalog)
{
    return createEphemerisEngine(catalog.bodies());
}

std::unique_ptr<IEphemerisEngine> createEphemerisEngine(std::initializer_list<CelestialBody> bodies)
{
    return createEphemerisEngine(std::span<const CelestialBody>{bodies.begin(), bodies.size()});
}

std::unique_ptr<IEphemerisEngine> createEphemerisEngine(std::span<const CelestialBody> bodies)
{
    EphemerisEngineFactoryRequest request;
    request.catalogBodies = bodies;
    request.options = simpleEngineDefaultOptions();
    EphemerisEngineFactoryResult result = createEphemerisEngine(request);
    return std::move(result.engine);
}

}  // namespace skygate::ephemeris
