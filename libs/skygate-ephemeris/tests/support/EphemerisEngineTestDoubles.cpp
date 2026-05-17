#include "EphemerisEngineTestDoubles.hpp"

#include <utility>

namespace skygate::ephemeris::tests {

[[nodiscard]] CelestialBody
makeFixedAltitudeBody(std::string id, const double rightAscensionHours, const double declinationDeg)
{
    CelestialBody body;
    body.id = std::move(id);
    body.displayName = body.id;
    body.type = CelestialBodyType::Star;
    body.ephemerisSource = CelestialBodyEphemerisSource::FixedEquatorial;
    body.fixedEquatorial = skygate::core::EquatorialCoordinate{
        .rightAscensionHours = rightAscensionHours,
        .declinationDeg = declinationDeg,
    };
    return body;
}

FixedAltitudeEngine::FixedAltitudeEngine(const double altitudeDeg)
    : FixedAltitudeEngine(
          std::vector<FixedAltitudeBody>{
              FixedAltitudeBody{
                  .body = makeFixedAltitudeBody("target"),
                  .altitudeDeg = altitudeDeg,
              },
          }
      )
{
}

FixedAltitudeEngine::FixedAltitudeEngine(std::vector<FixedAltitudeBody> bodies)
    : m_bodies(std::make_shared<const std::vector<FixedAltitudeBody>>(std::move(bodies)))
{
    std::vector<CelestialBody> catalogBodies;
    catalogBodies.reserve(m_bodies->size());
    for (const FixedAltitudeBody& fixedBody : *m_bodies) {
        catalogBodies.push_back(fixedBody.body);
    }
    m_catalogBodies = std::make_shared<const std::vector<CelestialBody>>(std::move(catalogBodies));
}

SkySnapshot FixedAltitudeEngine::compute(const EphemerisRequest& request) const
{
    return compute(request.context);
}

std::optional<CelestialBodyState>
FixedAltitudeEngine::computeBodyState(const EphemerisRequest& request, const std::string_view bodyId) const
{
    return computeBodyState(request.context, bodyId);
}

std::optional<CelestialBodyState>
FixedAltitudeEngine::computeBodyState(const EphemerisRequest& request, const std::size_t bodyIndex) const
{
    return computeBodyState(request.context, static_cast<std::uint32_t>(bodyIndex));
}

SkySnapshot FixedAltitudeEngine::compute(const skygate::core::SkyContext& context) const
{
    SkySnapshot snapshot;
    snapshot.context = context;
    snapshot.catalogBodies = m_catalogBodies;
    snapshot.states.reserve(m_bodies->size());
    for (std::uint32_t bodyIndex = 0; bodyIndex < m_bodies->size(); ++bodyIndex) {
        snapshot.states.push_back(stateFor(bodyIndex));
    }
    return snapshot;
}

std::optional<CelestialBodyState>
FixedAltitudeEngine::computeBodyState(const skygate::core::SkyContext&, const std::string_view bodyId) const
{
    for (std::uint32_t bodyIndex = 0; bodyIndex < m_bodies->size(); ++bodyIndex) {
        if ((*m_bodies)[bodyIndex].body.id == bodyId) {
            return stateFor(bodyIndex);
        }
    }
    return std::nullopt;
}

std::optional<CelestialBodyState>
FixedAltitudeEngine::computeBodyState(const skygate::core::SkyContext&, const std::uint32_t bodyIndex) const
{
    if (bodyIndex >= m_bodies->size()) {
        return std::nullopt;
    }
    return stateFor(bodyIndex);
}

CelestialBodyState FixedAltitudeEngine::stateFor(const std::uint32_t bodyIndex) const
{
    const FixedAltitudeBody& fixedBody = (*m_bodies)[bodyIndex];
    return CelestialBodyState{
        .bodyIndex = bodyIndex,
        .equatorial =
            {
                .rightAscensionHours = fixedBody.body.fixedEquatorial->rightAscensionHours,
                .declinationDeg = fixedBody.body.fixedEquatorial->declinationDeg,
            },
        .horizontal = {
            .altitudeDeg = fixedBody.altitudeDeg,
            .azimuthDeg = fixedBody.azimuthDeg,
        },
    };
}

RequestCountingEphemerisEngine::RequestCountingEphemerisEngine(
    std::unique_ptr<IEphemerisEngine> engine,
    EphemerisEngineOptions options,
    std::shared_ptr<const std::vector<CelestialBody>> catalogBodies
)
    : m_engine(std::move(engine)), m_options(options), m_catalogBodies(std::move(catalogBodies))
{
    if (m_engine != nullptr && m_options.engineKind == EphemerisEngineKind::Simple
        && m_options.correctionFlags == EphemerisCorrectionFlags::NoCorrections) {
        m_options = m_engine->options();
    }
}

EphemerisEngineKind RequestCountingEphemerisEngine::kind() const noexcept
{
    return m_options.engineKind;
}

EphemerisEngineOptions RequestCountingEphemerisEngine::options() const noexcept
{
    return m_options;
}

SkySnapshot RequestCountingEphemerisEngine::compute(const EphemerisRequest& request) const
{
    SkySnapshot snapshot = m_engine->compute(request);
    attachCatalogBodies(snapshot);
    return snapshot;
}

std::optional<CelestialBodyState>
RequestCountingEphemerisEngine::computeBodyState(const EphemerisRequest& request, const std::string_view bodyId) const
{
    ++m_requestSampleCount;
    return m_engine->computeBodyState(request, bodyId);
}

std::optional<CelestialBodyState>
RequestCountingEphemerisEngine::computeBodyState(const EphemerisRequest& request, const std::size_t bodyIndex) const
{
    ++m_requestSampleCount;
    return m_engine->computeBodyState(request, bodyIndex);
}

SkySnapshot RequestCountingEphemerisEngine::compute(const skygate::core::SkyContext& context) const
{
    SkySnapshot snapshot = m_engine->compute(context);
    attachCatalogBodies(snapshot);
    return snapshot;
}

std::optional<CelestialBodyState> RequestCountingEphemerisEngine::computeBodyState(
    const skygate::core::SkyContext& context, const std::string_view bodyId
) const
{
    ++m_contextSampleCount;
    return m_engine->computeBodyState(context, bodyId);
}

std::optional<CelestialBodyState> RequestCountingEphemerisEngine::computeBodyState(
    const skygate::core::SkyContext& context, const std::uint32_t bodyIndex
) const
{
    ++m_contextSampleCount;
    return m_engine->computeBodyState(context, bodyIndex);
}

int RequestCountingEphemerisEngine::requestSampleCount() const noexcept
{
    return m_requestSampleCount;
}

int RequestCountingEphemerisEngine::contextSampleCount() const noexcept
{
    return m_contextSampleCount;
}

void RequestCountingEphemerisEngine::attachCatalogBodies(SkySnapshot& snapshot) const
{
    if (m_catalogBodies != nullptr) {
        snapshot.catalogBodies = m_catalogBodies;
    }
}

EphemerisEngineOptions highPrecisionLightTimeOptions() noexcept
{
    EphemerisEngineOptions options;
    options.engineKind = EphemerisEngineKind::HighPrecision;
    options.correctionFlags = EphemerisCorrectionFlags::LightTime;
    return options;
}

}  // namespace skygate::ephemeris::tests
