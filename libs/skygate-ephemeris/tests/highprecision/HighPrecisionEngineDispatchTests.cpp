#include "BaseCelestialBody.hpp"
#include "EphemerisRequest.hpp"
#include "EphemerisSnapshot.hpp"
#include "HighPrecisionEngineFixture.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "PassThroughApparentPlaceCalculator.hpp"
#include "ValidUtcTimeScaleService.hpp"
#include "engine/EphemerisCapabilities.hpp"
#include "engine/EphemerisCorrectionFlags.hpp"
#include "engine/EphemerisEngineKind.hpp"
#include "engine/EphemerisEngineOptions.hpp"
#include "engine/EphemerisEngineQueryStatus.hpp"
#include "engine/EphemerisEngineWarning.hpp"
#include "engine/IEarthOrientationProvider.hpp"
#include "engine/IEphemerisEngine.hpp"
#include "engine/ITimeScaleService.hpp"
#include "engine/highprecision/ApparentPlaceCalculator.hpp"
#include "engine/highprecision/CatalogStarAstrometryArrays.hpp"
#include "engine/highprecision/EphemerisComputationCache.hpp"
#include "engine/highprecision/HighPrecisionCalculatorResult.hpp"
#include "engine/highprecision/HighPrecisionComputationInput.hpp"
#include "engine/highprecision/HighPrecisionEphemerisEngine.hpp"
#include "engine/highprecision/IApparentPlaceCalculator.hpp"
#include "engine/highprecision/IAtmosphericRefractionCalculator.hpp"
#include "engine/highprecision/IEphemerisResultBuilder.hpp"
#include "engine/highprecision/IFrameTransformer.hpp"
#include "engine/highprecision/IStarAstrometryCalculator.hpp"
#include "engine/highprecision/ISolarSystemStateCalculator.hpp"
#include "engine/highprecision/PreparedEphemerisRequestState.hpp"
#include "engine/highprecision/StarAstrometryBatchResult.hpp"
#include "engine/highprecision/StarAstrometryCalculator.hpp"

#include <QtTest/QtTest>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

using skygate::ephemeris::tests::HighPrecisionEngineFixture;
using skygate::ephemeris::tests::PassThroughApparentPlaceCalculator;
using skygate::ephemeris::tests::ValidUtcTimeScaleService;

namespace {

using namespace skygate::ephemeris;
using namespace skygate::ephemeris::highprecision;
using namespace skygate::core;

[[nodiscard]] OwnGalaxyCelestialBody makeUnsupportedBody()
{
    OwnGalaxyCelestialBody body;
    body.id = "unresolved";
    body.displayName = "Unresolved";
    body.kind = BaseCelestialBody::Kind::Constellation;
    return body;
}

class RecordingSolarSystemCalculator final : public ISolarSystemStateCalculator {
public:
    [[nodiscard]] HighPrecisionCalculatorResult calculate(const HighPrecisionComputationInput& input) const override
    {
        ++m_callCount;
        m_lastBodyId = input.body.id;
        m_lastFlags = input.request.options.correctionFlags();
        return HighPrecisionEngineFixture::calculatorResult(input, 1.25, -2.5, "solar-system fake");
    }

    [[nodiscard]] int callCount() const noexcept
    {
        return m_callCount;
    }

    [[nodiscard]] std::string lastBodyId() const
    {
        return m_lastBodyId;
    }

    [[nodiscard]] EphemerisCorrectionFlags lastFlags() const noexcept
    {
        return m_lastFlags;
    }

private:
    mutable int m_callCount = 0;
    mutable std::string m_lastBodyId;
    mutable EphemerisCorrectionFlags m_lastFlags = EphemerisCorrectionFlags::noCorrections();
};

class RecordingStarAstrometryCalculator final : public IStarAstrometryCalculator {
public:
    [[nodiscard]] HighPrecisionCalculatorResult calculate(const HighPrecisionComputationInput& input) const override
    {
        ++m_callCount;
        m_lastBodyId = input.body.id;
        m_lastFlags = input.request.options.correctionFlags();
        return HighPrecisionEngineFixture::calculatorResult(input, 3.5, 42.0, "star-astrometry fake");
    }

    [[nodiscard]] std::vector<StarAstrometryBatchResult> calculateBatch(
        const EphemerisRequest& request,
        const CatalogStarAstrometryArrays& arrays,
        std::shared_ptr<const PreparedEphemerisRequestState> preparedRequestState = {}
    ) const override
    {
        Q_UNUSED(request);
        Q_UNUSED(arrays);
        Q_UNUSED(preparedRequestState);
        return {};
    }

    [[nodiscard]] int callCount() const noexcept
    {
        return m_callCount;
    }

    [[nodiscard]] std::string lastBodyId() const
    {
        return m_lastBodyId;
    }

    [[nodiscard]] EphemerisCorrectionFlags lastFlags() const noexcept
    {
        return m_lastFlags;
    }

private:
    mutable int m_callCount = 0;
    mutable std::string m_lastBodyId;
    mutable EphemerisCorrectionFlags m_lastFlags = EphemerisCorrectionFlags::noCorrections();
};

class BatchRecordingStarAstrometryCalculator final : public IStarAstrometryCalculator {
public:
    [[nodiscard]] HighPrecisionCalculatorResult calculate(const HighPrecisionComputationInput& input) const override
    {
        ++m_singleCallCount;
        return HighPrecisionEngineFixture::calculatorResult(input, 21.0, -21.0, "single-star fallback");
    }

    [[nodiscard]] std::vector<StarAstrometryBatchResult> calculateBatch(
        const EphemerisRequest& request,
        const CatalogStarAstrometryArrays& arrays,
        std::shared_ptr<const PreparedEphemerisRequestState> preparedRequestState = {}
    ) const override
    {
        static_cast<void>(preparedRequestState);
        ++m_batchCallCount;
        m_lastBatchSize = arrays.size();
        m_lastRequestPart2 = request.epoch.julianDatePart2;

        std::vector<StarAstrometryBatchResult> results;
        results.reserve(arrays.size());
        for (std::size_t arrayIndex = 0; arrayIndex < arrays.size(); ++arrayIndex) {
            HighPrecisionCalculatorResult result;
            result.equatorial = EquatorialCoordinate{
                .rightAscensionHours =
                    arrays.referenceRightAscensionHours()[arrayIndex] + request.epoch.julianDatePart2,
                .declinationDeg = arrays.referenceDeclinationDegrees()[arrayIndex],
            };
            result.metadata.status = skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid;
            result.metadata.dataSourceProvenance = "batch star astrometry";
            results.push_back(
                StarAstrometryBatchResult{
                    .bodyIndex = arrays.bodyIndices()[arrayIndex],
                    .result = result,
                }
            );
        }
        return results;
    }

    [[nodiscard]] int singleCallCount() const noexcept
    {
        return m_singleCallCount;
    }

    [[nodiscard]] int batchCallCount() const noexcept
    {
        return m_batchCallCount;
    }

    [[nodiscard]] std::size_t lastBatchSize() const noexcept
    {
        return m_lastBatchSize;
    }

    [[nodiscard]] double lastRequestPart2() const noexcept
    {
        return m_lastRequestPart2;
    }

private:
    mutable int m_singleCallCount = 0;
    mutable int m_batchCallCount = 0;
    mutable std::size_t m_lastBatchSize = 0U;
    mutable double m_lastRequestPart2 = 0.0;
};

class RecordingApparentPlaceCalculator final : public IApparentPlaceCalculator {
public:
    [[nodiscard]] HighPrecisionCalculatorResult apply(
        const HighPrecisionComputationInput& input, const HighPrecisionCalculatorResult& calculatorResult
    ) const override
    {
        ++m_callCount;
        m_lastFlags = input.request.options.correctionFlags();

        HighPrecisionCalculatorResult result = calculatorResult;
        if (result.equatorial.has_value()) {
            result.equatorial->rightAscensionHours += 10.0;
        }
        result.metadata.appliedCorrections = input.request.options.correctionFlags();
        return result;
    }

    [[nodiscard]] std::vector<StarAstrometryBatchResult> applyBatch(
        const EphemerisRequest& request,
        std::span<const BaseCelestialBody* const> bodies,
        std::span<const StarAstrometryBatchResult> calculatorResults,
        std::shared_ptr<const PreparedEphemerisRequestState> preparedRequestState = {}
    ) const override
    {
        std::vector<StarAstrometryBatchResult> results;
        results.reserve(calculatorResults.size());
        for (const StarAstrometryBatchResult& calculatorResult : calculatorResults) {
            if (calculatorResult.bodyIndex >= bodies.size() || bodies[calculatorResult.bodyIndex] == nullptr) {
                continue;
            }

            const HighPrecisionComputationInput input{
                .request = request,
                .body = *bodies[calculatorResult.bodyIndex],
                .preparedRequestState = preparedRequestState,
                .bodyIndex = calculatorResult.bodyIndex,
            };
            results.push_back(
                StarAstrometryBatchResult{
                    .bodyIndex = calculatorResult.bodyIndex,
                    .result = apply(input, calculatorResult.result),
                }
            );
        }
        return results;
    }

    [[nodiscard]] int callCount() const noexcept
    {
        return m_callCount;
    }

    [[nodiscard]] EphemerisCorrectionFlags lastFlags() const noexcept
    {
        return m_lastFlags;
    }

private:
    mutable int m_callCount = 0;
    mutable EphemerisCorrectionFlags m_lastFlags = EphemerisCorrectionFlags::noCorrections();
};

class BatchRecordingFrameTransformer final : public IFrameTransformer {
public:
    [[nodiscard]] std::vector<CelestialFrameTransformResult>
    transform(const CelestialFrameTransformRequest& request) const override
    {
        ++m_batchCallCount;
        m_lastBatchSize = request.vectors.size();

        std::vector<CelestialFrameTransformResult> results;
        results.reserve(request.vectors.size());
        for (const Vector3d& vector : request.vectors) {
            results.push_back(transform(vector));
        }
        return results;
    }

    [[nodiscard]] int singleCallCount() const noexcept
    {
        return 0;
    }

    [[nodiscard]] int batchCallCount() const noexcept
    {
        return m_batchCallCount;
    }

    [[nodiscard]] std::size_t lastBatchSize() const noexcept
    {
        return m_lastBatchSize;
    }

private:
    [[nodiscard]] static CelestialFrameTransformResult transform(const Vector3d& vector)
    {
        CelestialFrameTransformResult result;
        result.vector = vector;
        result.metadata.status = skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid;
        result.metadata.appliedCorrections = EphemerisCorrectionFlags::precessionNutation();
        result.metadata.dataSourceProvenance = "batch frame transformer fake";
        return result;
    }

    mutable int m_batchCallCount = 0;
    mutable std::size_t m_lastBatchSize = 0U;
};

class RecordingResultBuilder final : public IEphemerisResultBuilder {
public:
    [[nodiscard]] CelestialBodyState buildState(
        const HighPrecisionComputationInput& input, const HighPrecisionCalculatorResult& calculatorResult
    ) const override
    {
        ++m_stateCount;
        m_lastBodyId = input.body.id;
        m_lastFlags = input.request.options.correctionFlags();

        CelestialBodyState state = makeEmptyState(input.bodyIndex);
        if (calculatorResult.equatorial.has_value()) {
            state.equatorial = *calculatorResult.equatorial;
        }
        if (calculatorResult.horizontal.has_value()) {
            state.horizontal = *calculatorResult.horizontal;
        }
        state.metadata = calculatorResult.metadata;
        state.metadata.estimatedAngularUncertaintyArcsec = 0.42;
        return state;
    }

    [[nodiscard]] CelestialBodyState buildUnsupportedState(const HighPrecisionComputationInput& input) const override
    {
        ++m_unsupportedCount;
        CelestialBodyState state = makeEmptyState(input.bodyIndex);
        state.metadata.status = skygate::ephemeris::EphemerisEngineQueryStatus::Type::Unsupported;
        state.metadata.addWarning(EphemerisEngineWarning::Code::UnsupportedBody);
        state.metadata.dataSourceProvenance = "result-builder fake";
        return state;
    }

    [[nodiscard]] CelestialBodyState buildFailedState(const HighPrecisionComputationInput& input) const override
    {
        ++m_failedCount;
        CelestialBodyState state = makeEmptyState(input.bodyIndex);
        state.metadata.status = skygate::ephemeris::EphemerisEngineQueryStatus::Type::Failed;
        state.metadata.addWarning(EphemerisEngineWarning::Code::ComputationFailed);
        state.metadata.dataSourceProvenance = "result-builder fake";
        return state;
    }

    [[nodiscard]] int stateCount() const noexcept
    {
        return m_stateCount;
    }

    [[nodiscard]] int unsupportedCount() const noexcept
    {
        return m_unsupportedCount;
    }

    [[nodiscard]] int failedCount() const noexcept
    {
        return m_failedCount;
    }

    [[nodiscard]] std::string lastBodyId() const
    {
        return m_lastBodyId;
    }

    [[nodiscard]] EphemerisCorrectionFlags lastFlags() const noexcept
    {
        return m_lastFlags;
    }

private:
    [[nodiscard]] static CelestialBodyState makeEmptyState(const std::size_t bodyIndex) noexcept
    {
        CelestialBodyState state;
        state.bodyIndex = static_cast<std::uint32_t>(bodyIndex);
        state.equatorial.rightAscensionHours = std::numeric_limits<double>::quiet_NaN();
        state.equatorial.declinationDeg = std::numeric_limits<double>::quiet_NaN();
        state.horizontal.altitudeDeg = std::numeric_limits<double>::quiet_NaN();
        state.horizontal.azimuthDeg = std::numeric_limits<double>::quiet_NaN();
        return state;
    }

    mutable int m_stateCount = 0;
    mutable int m_unsupportedCount = 0;
    mutable int m_failedCount = 0;
    mutable std::string m_lastBodyId;
    mutable EphemerisCorrectionFlags m_lastFlags = EphemerisCorrectionFlags::noCorrections();
};

class StubEarthOrientationProvider final : public skygate::ephemeris::IEarthOrientationProvider {
public:
    [[nodiscard]] const IEarthOrientationProvider::DataInfo& dataInfo() const noexcept override
    {
        return m_dataInfo;
    }

    [[nodiscard]] std::span<const IEarthOrientationProvider::TableEntry> entries() const noexcept override
    {
        return {};
    }

private:
    IEarthOrientationProvider::DataInfo m_dataInfo;
};

class StubAtmosphericRefractionCalculator final : public IAtmosphericRefractionCalculator {
public:
    [[nodiscard]] HighPrecisionCalculatorResult
    apply(const HighPrecisionComputationInput&, const HighPrecisionCalculatorResult& calculatorResult) const override
    {
        return calculatorResult;
    }
};

}  // namespace

class HighPrecisionEngineDispatchTests final : public QObject {
    Q_OBJECT

private slots:
    void exposesMetadataAndCapabilities();
    void dispatchesSolarSystemAndStarBodies();
    void singleBodyOverloadsShareResultsAndCaches_data();
    void singleBodyOverloadsShareResultsAndCaches();
    void failsRequestedCorrectionsWithoutApparentPlace();
    void usesBatchStarPathForFullFrameSnapshot();
    void batchesLargeCatalogWithoutSingleStarDispatch();
    void batchesLargeCatalogApparentPlaceTransformsOnce();
    void batchesTopocentricApparentPlaceRequestWideState();
    void fallsBackToSingleStarWhenBatchIsEmpty();
    void forwardsOptionsThroughCollaboratorsAndBuilder();
    void bypassesApparentPlaceForGeometricSolarSystem();
    void bypassesApparentPlaceForGeometricStarRequests();
    void validatesRequestsBeforeDispatchingCalculators();
    void returnsStructuredUnsupportedStatus();
};

void HighPrecisionEngineDispatchTests::exposesMetadataAndCapabilities()
{
    const std::array bodies{HighPrecisionEngineFixture::sunBody()};
    auto solarSystemCalculator = std::make_shared<RecordingSolarSystemCalculator>();
    auto starAstrometryCalculator = std::make_shared<RecordingStarAstrometryCalculator>();
    auto dependencies = HighPrecisionEngineFixture::dependencies(solarSystemCalculator, starAstrometryCalculator);
    dependencies.earthOrientationProvider = std::make_shared<StubEarthOrientationProvider>();
    dependencies.atmosphericRefractionCalculator = std::make_shared<StubAtmosphericRefractionCalculator>();

    EphemerisEngineOptions options;
    options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::Simple);
    options.setCorrectionFlags(EphemerisCorrectionFlags::apparentTopocentric());

    const HighPrecisionEphemerisEngine engine(
        HighPrecisionEngineFixture::catalog(bodies), options, std::move(dependencies)
    );

    QCOMPARE(
        static_cast<std::uint8_t>(engine.kind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision)
    );
    QVERIFY(engine.name() == std::string_view{"High-precision ephemeris engine"});
    QCOMPARE(
        static_cast<std::uint8_t>(engine.options().engineKind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision)
    );

    const EphemerisCapabilities capabilities = engine.capabilities();
    QVERIFY(
        skygate::ephemeris::EphemerisCapabilities::has(
            capabilities, skygate::ephemeris::EphemerisCapabilities::solarSystemBodies()
        )
    );
    QVERIFY(
        skygate::ephemeris::EphemerisCapabilities::has(
            capabilities, skygate::ephemeris::EphemerisCapabilities::catalogStars()
        )
    );
    QVERIFY(
        skygate::ephemeris::EphemerisCapabilities::has(
            capabilities, skygate::ephemeris::EphemerisCapabilities::topocentricPositions()
        )
    );
    QVERIFY(
        skygate::ephemeris::EphemerisCapabilities::has(
            capabilities, skygate::ephemeris::EphemerisCapabilities::atmosphericRefraction()
        )
    );
    QVERIFY(
        skygate::ephemeris::EphemerisCapabilities::has(
            capabilities, skygate::ephemeris::EphemerisCapabilities::extendedHistoricalRange()
        )
    );
    QCOMPARE(engine.supportedDateRanges().size(), std::size_t{1});
    QVERIFY(engine.dataSetInfo().id == std::string{"test-data"});
}

void HighPrecisionEngineDispatchTests::dispatchesSolarSystemAndStarBodies()
{
    const std::array bodies{HighPrecisionEngineFixture::sunBody(), HighPrecisionEngineFixture::starBody()};
    auto solarSystemCalculator = std::make_shared<RecordingSolarSystemCalculator>();
    auto starAstrometryCalculator = std::make_shared<RecordingStarAstrometryCalculator>();
    auto apparentPlaceCalculator = std::make_shared<RecordingApparentPlaceCalculator>();
    auto resultBuilder = std::make_shared<RecordingResultBuilder>();

    const HighPrecisionEphemerisEngine engine(
        HighPrecisionEngineFixture::catalog(bodies),
        HighPrecisionEngineFixture::request().options,
        HighPrecisionEngineFixture::dependencies(
            solarSystemCalculator, starAstrometryCalculator, apparentPlaceCalculator, resultBuilder
        )
    );

    const EphemerisSnapshot snapshot = engine.compute(HighPrecisionEngineFixture::request());

    QCOMPARE(snapshot.states.size(), std::size_t{2});
    QCOMPARE(solarSystemCalculator->callCount(), 1);
    QCOMPARE(starAstrometryCalculator->callCount(), 1);
    QCOMPARE(apparentPlaceCalculator->callCount(), 2);
    QCOMPARE(resultBuilder->stateCount(), 2);
    QVERIFY(solarSystemCalculator->lastBodyId() == std::string{"sun"});
    QVERIFY(starAstrometryCalculator->lastBodyId() == std::string{"vega"});
    QCOMPARE(snapshot.states[0].equatorial.rightAscensionHours, 11.25);
    QCOMPARE(snapshot.states[0].equatorial.declinationDeg, -2.5);
    QCOMPARE(snapshot.states[1].equatorial.rightAscensionHours, 13.5);
    QCOMPARE(snapshot.states[1].equatorial.declinationDeg, 42.0);
}

void HighPrecisionEngineDispatchTests::singleBodyOverloadsShareResultsAndCaches_data()
{
    QTest::addColumn<bool>("idFirst");
    QTest::addColumn<bool>("warmSnapshot");
    QTest::newRow("id-first") << true << false;
    QTest::newRow("index-first") << false << false;
    QTest::newRow("snapshot-id-first") << true << true;
    QTest::newRow("snapshot-index-first") << false << true;
}

void HighPrecisionEngineDispatchTests::singleBodyOverloadsShareResultsAndCaches()
{
    QFETCH(bool, idFirst);
    QFETCH(bool, warmSnapshot);
    const std::array bodies{HighPrecisionEngineFixture::sunBody(), HighPrecisionEngineFixture::starBody()};
    auto solar = std::make_shared<RecordingSolarSystemCalculator>();
    auto stars = std::make_shared<RecordingStarAstrometryCalculator>();
    const EphemerisRequest request = HighPrecisionEngineFixture::request();
    const HighPrecisionEphemerisEngine engine(
        HighPrecisionEngineFixture::catalog(bodies),
        request.options,
        HighPrecisionEngineFixture::dependencies(
            solar,
            stars,
            std::make_shared<RecordingApparentPlaceCalculator>(),
            {},
            std::make_shared<EphemerisComputationCache>()
        )
    );
    if (warmSnapshot) {
        static_cast<void>(engine.compute(request));
    }
    const std::array<std::string_view, 2> ids{"SuN", "VEGA"};
    for (std::size_t index = 0; index < ids.size(); ++index) {
        const auto first =
            idFirst ? engine.computeBodyState(request, ids[index]) : engine.computeBodyState(request, index);
        const auto second =
            idFirst ? engine.computeBodyState(request, index) : engine.computeBodyState(request, ids[index]);
        QVERIFY(first.has_value());
        QVERIFY(second.has_value());
        QCOMPARE(first->bodyIndex, index);
        QCOMPARE(second->bodyIndex, first->bodyIndex);
        QCOMPARE(second->equatorial.rightAscensionHours, first->equatorial.rightAscensionHours);
        QCOMPARE(second->equatorial.declinationDeg, first->equatorial.declinationDeg);
        QCOMPARE(second->metadata.status, first->metadata.status);
        QCOMPARE(second->metadata.warningCodeMask, first->metadata.warningCodeMask);
        QCOMPARE(second->metadata.dataSourceProvenance, first->metadata.dataSourceProvenance);
        QCOMPARE(second->metadata.requestedCorrections, first->metadata.requestedCorrections);
        QCOMPARE(second->metadata.appliedCorrections, first->metadata.appliedCorrections);
        QCOMPARE(second->metadata.unavailableCorrections, first->metadata.unavailableCorrections);
        QCOMPARE(second->metadata.skippedCorrections, first->metadata.skippedCorrections);
    }
    QVERIFY(!engine.computeBodyState(request, "").has_value());
    QVERIFY(!engine.computeBodyState(request, "unknown").has_value());
    QVERIFY(!engine.computeBodyState(request, bodies.size()).has_value());
    QVERIFY(!engine.computeBodyState(request, std::numeric_limits<std::size_t>::max()).has_value());
    QCOMPARE(solar->callCount(), 1);
    QCOMPARE(stars->callCount(), 1);
}

void HighPrecisionEngineDispatchTests::failsRequestedCorrectionsWithoutApparentPlace()
{
    const std::array bodies{
        HighPrecisionEngineFixture::sunBody(),
        HighPrecisionEngineFixture::fixedStarBody("star-a", 2.0, 10.0),
    };
    auto solarSystemCalculator = std::make_shared<RecordingSolarSystemCalculator>();
    auto starAstrometryCalculator = std::make_shared<BatchRecordingStarAstrometryCalculator>();

    const EphemerisRequest request = HighPrecisionEngineFixture::request();
    const HighPrecisionEphemerisEngine engine(
        HighPrecisionEngineFixture::catalog(bodies),
        request.options,
        HighPrecisionEngineFixture::dependencies(solarSystemCalculator, starAstrometryCalculator)
    );

    const EphemerisSnapshot snapshot = engine.compute(request);

    QCOMPARE(snapshot.states.size(), std::size_t{2});
    QCOMPARE(solarSystemCalculator->callCount(), 1);
    QCOMPARE(starAstrometryCalculator->batchCallCount(), 1);
    for (const CelestialBodyState& state : snapshot.states) {
        QCOMPARE(
            static_cast<std::uint8_t>(state.metadata.status),
            static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Failed)
        );
        QVERIFY(state.metadata.hasWarning(EphemerisEngineWarning::Code::ComputationFailed));
        QVERIFY(state.metadata.hasWarning(EphemerisEngineWarning::Code::CorrectionUnavailable));
        QCOMPARE(state.metadata.requestedCorrections, request.options.correctionFlags());
        QCOMPARE(state.metadata.unavailableCorrections, request.options.correctionFlags());
    }
}

void HighPrecisionEngineDispatchTests::usesBatchStarPathForFullFrameSnapshot()
{
    const std::array bodies{
        HighPrecisionEngineFixture::sunBody(),
        HighPrecisionEngineFixture::fixedStarBody("star-a", 2.0, 10.0),
        HighPrecisionEngineFixture::fixedStarBody("star-b", 5.0, -20.0),
        makeUnsupportedBody(),
    };
    auto solarSystemCalculator = std::make_shared<RecordingSolarSystemCalculator>();
    auto starAstrometryCalculator = std::make_shared<BatchRecordingStarAstrometryCalculator>();
    auto apparentPlaceCalculator = std::make_shared<RecordingApparentPlaceCalculator>();

    const HighPrecisionEphemerisEngine engine(
        HighPrecisionEngineFixture::catalog(bodies),
        HighPrecisionEngineFixture::request().options,
        HighPrecisionEngineFixture::dependencies(
            solarSystemCalculator, starAstrometryCalculator, apparentPlaceCalculator
        )
    );

    EphemerisRequest firstRequest = HighPrecisionEngineFixture::request();
    firstRequest.epoch.julianDatePart2 = 0.25;
    const EphemerisSnapshot firstSnapshot = engine.compute(firstRequest);

    QCOMPARE(firstSnapshot.states.size(), std::size_t{4});
    QCOMPARE(starAstrometryCalculator->batchCallCount(), 1);
    QCOMPARE(starAstrometryCalculator->singleCallCount(), 0);
    QCOMPARE(starAstrometryCalculator->lastBatchSize(), std::size_t{2});
    QCOMPARE(solarSystemCalculator->callCount(), 1);
    QCOMPARE(apparentPlaceCalculator->callCount(), 3);
    QCOMPARE(firstSnapshot.states[1].bodyIndex, std::uint32_t{1});
    QCOMPARE(firstSnapshot.states[2].bodyIndex, std::uint32_t{2});
    QCOMPARE(firstSnapshot.states[1].equatorial.rightAscensionHours, 12.25);
    QCOMPARE(firstSnapshot.states[1].equatorial.declinationDeg, 10.0);
    QCOMPARE(firstSnapshot.states[2].equatorial.rightAscensionHours, 15.25);
    QCOMPARE(firstSnapshot.states[2].equatorial.declinationDeg, -20.0);
    QCOMPARE(
        firstSnapshot.states[3].metadata.status, skygate::ephemeris::EphemerisEngineQueryStatus::Type::Unsupported
    );

    EphemerisRequest secondRequest = firstRequest;
    secondRequest.epoch.julianDatePart2 = 0.75;
    const EphemerisSnapshot secondSnapshot = engine.compute(secondRequest);

    QCOMPARE(starAstrometryCalculator->batchCallCount(), 2);
    QCOMPARE(starAstrometryCalculator->singleCallCount(), 0);
    QCOMPARE(starAstrometryCalculator->lastRequestPart2(), 0.75);
    QCOMPARE(secondSnapshot.states[1].equatorial.rightAscensionHours, 12.75);
    QCOMPARE(secondSnapshot.states[2].equatorial.rightAscensionHours, 15.75);
}

void HighPrecisionEngineDispatchTests::batchesLargeCatalogWithoutSingleStarDispatch()
{
    constexpr std::size_t kRepresentativeCatalogSize = 4'096U;
    std::vector<OwnGalaxyCelestialBody> bodies;
    bodies.reserve(kRepresentativeCatalogSize);
    for (std::size_t index = 0; index < kRepresentativeCatalogSize; ++index) {
        bodies.push_back(
            HighPrecisionEngineFixture::fixedStarBody(
                "star-" + std::to_string(index), 1.0 + static_cast<double>(index) * 0.001, 5.0
            )
        );
    }
    auto starAstrometryCalculator = std::make_shared<BatchRecordingStarAstrometryCalculator>();

    const HighPrecisionEphemerisEngine engine(
        HighPrecisionEngineFixture::catalog(bodies),
        HighPrecisionEngineFixture::request().options,
        HighPrecisionEngineFixture::dependencies(
            {}, starAstrometryCalculator, PassThroughApparentPlaceCalculator::create()
        )
    );

    const EphemerisSnapshot snapshot = engine.compute(HighPrecisionEngineFixture::request());

    QCOMPARE(snapshot.states.size(), kRepresentativeCatalogSize);
    QCOMPARE(starAstrometryCalculator->batchCallCount(), 1);
    QCOMPARE(starAstrometryCalculator->lastBatchSize(), kRepresentativeCatalogSize);
    QCOMPARE(starAstrometryCalculator->singleCallCount(), 0);
    QCOMPARE(snapshot.states.front().bodyIndex, std::uint32_t{0});
    QCOMPARE(snapshot.states.back().bodyIndex, static_cast<std::uint32_t>(kRepresentativeCatalogSize - 1U));
}

void HighPrecisionEngineDispatchTests::batchesLargeCatalogApparentPlaceTransformsOnce()
{
    constexpr std::size_t kRepresentativeCatalogSize = 4'096U;
    std::vector<OwnGalaxyCelestialBody> bodies;
    bodies.reserve(kRepresentativeCatalogSize);
    for (std::size_t index = 0; index < kRepresentativeCatalogSize; ++index) {
        bodies.push_back(
            HighPrecisionEngineFixture::fixedStarBody(
                "star-" + std::to_string(index), 1.0 + static_cast<double>(index) * 0.001, 5.0
            )
        );
    }

    auto starAstrometryCalculator = std::make_shared<StarAstrometryCalculator>();
    auto frameTransformer = std::make_shared<BatchRecordingFrameTransformer>();
    auto apparentPlaceCalculator = std::make_shared<ApparentPlaceCalculator>(frameTransformer, nullptr, nullptr);

    const HighPrecisionEphemerisEngine engine(
        HighPrecisionEngineFixture::catalog(bodies),
        HighPrecisionEngineFixture::request().options,
        HighPrecisionEngineFixture::dependencies({}, starAstrometryCalculator, apparentPlaceCalculator)
    );

    const EphemerisSnapshot snapshot = engine.compute(HighPrecisionEngineFixture::request());

    QCOMPARE(snapshot.states.size(), kRepresentativeCatalogSize);
    QCOMPARE(frameTransformer->batchCallCount(), 1);
    QCOMPARE(frameTransformer->lastBatchSize(), kRepresentativeCatalogSize);
    QCOMPARE(frameTransformer->singleCallCount(), 0);
    QCOMPARE(snapshot.states.front().bodyIndex, std::uint32_t{0});
    QCOMPARE(snapshot.states.back().bodyIndex, static_cast<std::uint32_t>(kRepresentativeCatalogSize - 1U));
}

void HighPrecisionEngineDispatchTests::batchesTopocentricApparentPlaceRequestWideState()
{
    constexpr std::size_t kRepresentativeCatalogSize = 4'096U;
    std::vector<OwnGalaxyCelestialBody> bodies;
    bodies.reserve(kRepresentativeCatalogSize);
    for (std::size_t index = 0; index < kRepresentativeCatalogSize; ++index) {
        bodies.push_back(
            HighPrecisionEngineFixture::fixedStarBody(
                "star-" + std::to_string(index), 1.0 + static_cast<double>(index) * 0.001, 5.0
            )
        );
    }

    auto timeScaleService = std::make_shared<ValidUtcTimeScaleService>();
    auto starAstrometryCalculator = std::make_shared<StarAstrometryCalculator>();
    auto frameTransformer = std::make_shared<BatchRecordingFrameTransformer>();
    auto apparentPlaceCalculator = std::make_shared<ApparentPlaceCalculator>(
        frameTransformer, timeScaleService, HighPrecisionEngineFixture::earthOrientationProvider()
    );

    EphemerisRequest request = HighPrecisionEngineFixture::request();
    request.options.setCorrectionFlags(EphemerisCorrectionFlags::topocentric());
    const HighPrecisionEphemerisEngine engine(
        HighPrecisionEngineFixture::catalog(bodies),
        request.options,
        HighPrecisionEngineFixture::dependencies({}, starAstrometryCalculator, apparentPlaceCalculator)
    );

    const EphemerisSnapshot snapshot = engine.compute(request);

    QCOMPARE(snapshot.states.size(), kRepresentativeCatalogSize);
    QCOMPARE(timeScaleService->convertCallCount(), 1);
    QCOMPARE(frameTransformer->batchCallCount(), 3);
    QCOMPARE(frameTransformer->lastBatchSize(), kRepresentativeCatalogSize);
    QCOMPARE(frameTransformer->singleCallCount(), 0);
    QCOMPARE(snapshot.states.front().bodyIndex, std::uint32_t{0});
    QCOMPARE(snapshot.states.back().bodyIndex, static_cast<std::uint32_t>(kRepresentativeCatalogSize - 1U));
}

void HighPrecisionEngineDispatchTests::fallsBackToSingleStarWhenBatchIsEmpty()
{
    const std::array bodies{
        HighPrecisionEngineFixture::fixedStarBody("star-a", 2.0, 10.0),
        HighPrecisionEngineFixture::fixedStarBody("star-b", 5.0, -20.0),
    };
    auto starAstrometryCalculator = std::make_shared<RecordingStarAstrometryCalculator>();

    const HighPrecisionEphemerisEngine engine(
        HighPrecisionEngineFixture::catalog(bodies),
        HighPrecisionEngineFixture::request().options,
        HighPrecisionEngineFixture::dependencies(
            {}, starAstrometryCalculator, PassThroughApparentPlaceCalculator::create()
        )
    );

    const EphemerisSnapshot snapshot = engine.compute(HighPrecisionEngineFixture::request());

    QCOMPARE(snapshot.states.size(), std::size_t{2});
    QCOMPARE(starAstrometryCalculator->callCount(), 2);
    QCOMPARE(snapshot.states[0].equatorial.rightAscensionHours, 3.5);
    QCOMPARE(snapshot.states[1].equatorial.rightAscensionHours, 3.5);
}

void HighPrecisionEngineDispatchTests::forwardsOptionsThroughCollaboratorsAndBuilder()
{
    const std::array bodies{HighPrecisionEngineFixture::sunBody()};
    auto solarSystemCalculator = std::make_shared<RecordingSolarSystemCalculator>();
    auto apparentPlaceCalculator = std::make_shared<RecordingApparentPlaceCalculator>();
    auto resultBuilder = std::make_shared<RecordingResultBuilder>();

    EphemerisRequest request = HighPrecisionEngineFixture::request();
    request.options.setCorrectionFlags(
        EphemerisCorrectionFlags::lightTime() | EphemerisCorrectionFlags::earthOrientation()
    );

    EphemerisEngineOptions engineOptions = request.options;
    engineOptions.setCorrectionFlags(EphemerisCorrectionFlags::atmosphericRefraction());

    const HighPrecisionEphemerisEngine engine(
        HighPrecisionEngineFixture::catalog(bodies),
        engineOptions,
        HighPrecisionEngineFixture::dependencies(solarSystemCalculator, {}, apparentPlaceCalculator, resultBuilder)
    );

    const auto state = engine.computeBodyState(request, std::size_t{0});

    QVERIFY(state.has_value());
    QCOMPARE(
        static_cast<std::uint32_t>(solarSystemCalculator->lastFlags()),
        static_cast<std::uint32_t>(request.options.correctionFlags())
    );
    QCOMPARE(
        static_cast<std::uint32_t>(apparentPlaceCalculator->lastFlags()),
        static_cast<std::uint32_t>(request.options.correctionFlags())
    );
    QCOMPARE(
        static_cast<std::uint32_t>(resultBuilder->lastFlags()),
        static_cast<std::uint32_t>(request.options.correctionFlags())
    );
    QCOMPARE(
        static_cast<std::uint32_t>(state->metadata.appliedCorrections),
        static_cast<std::uint32_t>(request.options.correctionFlags())
    );
    QCOMPARE(state->equatorial.rightAscensionHours, 11.25);
    QVERIFY(state->metadata.estimatedAngularUncertaintyArcsec.has_value());
    QCOMPARE(*state->metadata.estimatedAngularUncertaintyArcsec, 0.42);
}

void HighPrecisionEngineDispatchTests::bypassesApparentPlaceForGeometricSolarSystem()
{
    const std::array bodies{HighPrecisionEngineFixture::sunBody()};
    auto solarSystemCalculator = std::make_shared<RecordingSolarSystemCalculator>();
    auto apparentPlaceCalculator = std::make_shared<RecordingApparentPlaceCalculator>();
    auto resultBuilder = std::make_shared<RecordingResultBuilder>();

    EphemerisRequest request = HighPrecisionEngineFixture::request();
    request.options.setCorrectionFlags(EphemerisCorrectionFlags::geometric());

    const HighPrecisionEphemerisEngine engine(
        HighPrecisionEngineFixture::catalog(bodies),
        request.options,
        HighPrecisionEngineFixture::dependencies(solarSystemCalculator, {}, apparentPlaceCalculator, resultBuilder)
    );

    const auto state = engine.computeBodyState(request, std::size_t{0});

    QVERIFY(state.has_value());
    QCOMPARE(solarSystemCalculator->callCount(), 1);
    QCOMPARE(apparentPlaceCalculator->callCount(), 0);
    QCOMPARE(resultBuilder->stateCount(), 1);
    QCOMPARE(state->equatorial.rightAscensionHours, 1.25);
    QCOMPARE(state->equatorial.declinationDeg, -2.5);
    QCOMPARE(
        static_cast<std::uint32_t>(state->metadata.appliedCorrections),
        static_cast<std::uint32_t>(EphemerisCorrectionFlags::geometric())
    );
}

void HighPrecisionEngineDispatchTests::bypassesApparentPlaceForGeometricStarRequests()
{
    const std::array bodies{HighPrecisionEngineFixture::starBody()};
    auto starAstrometryCalculator = std::make_shared<RecordingStarAstrometryCalculator>();
    auto apparentPlaceCalculator = std::make_shared<RecordingApparentPlaceCalculator>();
    auto resultBuilder = std::make_shared<RecordingResultBuilder>();

    EphemerisRequest request = HighPrecisionEngineFixture::request();
    request.options.setCorrectionFlags(EphemerisCorrectionFlags::geometric());

    const HighPrecisionEphemerisEngine engine(
        HighPrecisionEngineFixture::catalog(bodies),
        request.options,
        HighPrecisionEngineFixture::dependencies({}, starAstrometryCalculator, apparentPlaceCalculator, resultBuilder)
    );

    const auto state = engine.computeBodyState(request, std::size_t{0});

    QVERIFY(state.has_value());
    QCOMPARE(starAstrometryCalculator->callCount(), 1);
    QCOMPARE(apparentPlaceCalculator->callCount(), 0);
    QCOMPARE(resultBuilder->stateCount(), 1);
    QCOMPARE(state->equatorial.rightAscensionHours, 3.5);
    QCOMPARE(state->equatorial.declinationDeg, 42.0);
    QCOMPARE(
        static_cast<std::uint32_t>(state->metadata.appliedCorrections),
        static_cast<std::uint32_t>(EphemerisCorrectionFlags::geometric())
    );
}

void HighPrecisionEngineDispatchTests::validatesRequestsBeforeDispatchingCalculators()
{
    const std::array bodies{HighPrecisionEngineFixture::sunBody()};
    auto solarSystemCalculator = std::make_shared<RecordingSolarSystemCalculator>();
    auto resultBuilder = std::make_shared<RecordingResultBuilder>();

    HighPrecisionEphemerisEngine engine(
        HighPrecisionEngineFixture::catalog(bodies),
        HighPrecisionEngineFixture::request().options,
        HighPrecisionEngineFixture::dependencies(solarSystemCalculator, {}, {}, resultBuilder)
    );

    EphemerisRequest request = HighPrecisionEngineFixture::request();
    request.epoch.julianDatePart1 = std::numeric_limits<double>::quiet_NaN();

    const auto state = engine.computeBodyState(request, "sun");

    QVERIFY(state.has_value());
    QCOMPARE(solarSystemCalculator->callCount(), 0);
    QCOMPARE(resultBuilder->failedCount(), 1);
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Failed)
    );
    QVERIFY(state->metadata.hasWarning(EphemerisEngineWarning::Code::ComputationFailed));
}

void HighPrecisionEngineDispatchTests::returnsStructuredUnsupportedStatus()
{
    const std::array bodies{makeUnsupportedBody()};
    auto solarSystemCalculator = std::make_shared<RecordingSolarSystemCalculator>();
    auto starAstrometryCalculator = std::make_shared<RecordingStarAstrometryCalculator>();
    auto resultBuilder = std::make_shared<RecordingResultBuilder>();

    const HighPrecisionEphemerisEngine engine(
        HighPrecisionEngineFixture::catalog(bodies),
        HighPrecisionEngineFixture::request().options,
        HighPrecisionEngineFixture::dependencies(solarSystemCalculator, starAstrometryCalculator, {}, resultBuilder)
    );

    const auto state = engine.computeBodyState(HighPrecisionEngineFixture::request(), std::size_t{0});

    QVERIFY(state.has_value());
    QCOMPARE(solarSystemCalculator->callCount(), 0);
    QCOMPARE(starAstrometryCalculator->callCount(), 0);
    QCOMPARE(resultBuilder->unsupportedCount(), 1);
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Unsupported)
    );
    QVERIFY(state->metadata.hasWarning(EphemerisEngineWarning::Code::UnsupportedBody));
    QVERIFY(!state->metadata.dataSourceProvenance.empty());
}

QTEST_APPLESS_MAIN(HighPrecisionEngineDispatchTests)

#include "HighPrecisionEngineDispatchTests.moc"
