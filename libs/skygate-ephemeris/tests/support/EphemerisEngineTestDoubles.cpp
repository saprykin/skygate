#include "EphemerisEngineTestDoubles.hpp"

#include <utility>

namespace skygate::ephemeris::tests {

[[nodiscard]] OwnGalaxyCelestialBody
makeFixedAltitudeBody(std::string id, const double rightAscensionHours, const double declinationDeg)
{
    OwnGalaxyCelestialBody body;
    body.id = std::move(id);
    body.displayName = body.id;
    body.kind = BaseCelestialBody::Kind::Star;
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
    std::vector<OwnGalaxyCelestialBody> catalogBodies;
    catalogBodies.reserve(m_bodies->size());
    for (const FixedAltitudeBody& fixedBody : *m_bodies) {
        catalogBodies.push_back(fixedBody.body);
    }
    m_catalogBodies = std::make_shared<const CelestialBodyCatalog>(std::move(catalogBodies));
}

EphemerisEngineKind::Type FixedAltitudeEngine::kind() const noexcept
{
    return EphemerisEngineKind::Type::Simple;
}

std::string_view FixedAltitudeEngine::name() const noexcept
{
    return "Fixed altitude test engine";
}

EphemerisSnapshot FixedAltitudeEngine::compute(const EphemerisRequest& request) const
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
    return computeBodyState(request.context, bodyIndex);
}

EphemerisSnapshot FixedAltitudeEngine::compute(const skygate::core::ObservationContext& context) const
{
    EphemerisSnapshot snapshot;
    snapshot.context = context;
    snapshot.catalogBodies = m_catalogBodies;
    snapshot.states.reserve(m_bodies->size());
    for (std::uint32_t bodyIndex = 0; bodyIndex < m_bodies->size(); ++bodyIndex) {
        snapshot.states.push_back(stateFor(bodyIndex));
    }
    return snapshot;
}

std::optional<CelestialBodyState>
FixedAltitudeEngine::computeBodyState(const skygate::core::ObservationContext&, const std::string_view bodyId) const
{
    for (std::uint32_t bodyIndex = 0; bodyIndex < m_bodies->size(); ++bodyIndex) {
        if ((*m_bodies)[bodyIndex].body.id == bodyId) {
            return stateFor(bodyIndex);
        }
    }
    return std::nullopt;
}

std::optional<CelestialBodyState>
FixedAltitudeEngine::computeBodyState(const skygate::core::ObservationContext&, const std::size_t bodyIndex) const
{
    if (bodyIndex >= m_bodies->size()) {
        return std::nullopt;
    }
    return stateFor(static_cast<std::uint32_t>(bodyIndex));
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
    std::shared_ptr<const CelestialBodyCatalog> catalogBodies
)
    : m_engine(std::move(engine)), m_options(options), m_catalogBodies(std::move(catalogBodies))
{
    if (m_engine != nullptr && m_options.engineKind() == skygate::ephemeris::EphemerisEngineKind::Type::Simple
        && m_options.correctionFlags() == EphemerisCorrectionFlags::noCorrections()) {
        m_options = m_engine->options();
    }
}

skygate::ephemeris::EphemerisEngineKind::Type RequestCountingEphemerisEngine::kind() const noexcept
{
    return m_options.engineKind();
}

std::string_view RequestCountingEphemerisEngine::name() const noexcept
{
    return "Request counting ephemeris engine";
}

EphemerisEngineTraits RequestCountingEphemerisEngine::traits() const noexcept
{
    return m_options.engineKind() == EphemerisEngineKind::Type::HighPrecision
               ? EphemerisEngineTraits::highPrecisionEngine()
               : EphemerisEngineTraits::noTraits();
}

EphemerisEngineOptions RequestCountingEphemerisEngine::options() const noexcept
{
    return m_options;
}

EphemerisSnapshot RequestCountingEphemerisEngine::compute(const EphemerisRequest& request) const
{
    EphemerisSnapshot snapshot = m_engine->compute(request);
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

EphemerisSnapshot RequestCountingEphemerisEngine::compute(const skygate::core::ObservationContext& context) const
{
    EphemerisSnapshot snapshot = m_engine->compute(context);
    attachCatalogBodies(snapshot);
    return snapshot;
}

std::optional<CelestialBodyState> RequestCountingEphemerisEngine::computeBodyState(
    const skygate::core::ObservationContext& context, const std::string_view bodyId
) const
{
    ++m_contextSampleCount;
    return m_engine->computeBodyState(context, bodyId);
}

std::optional<CelestialBodyState> RequestCountingEphemerisEngine::computeBodyState(
    const skygate::core::ObservationContext& context, const std::size_t bodyIndex
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

void RequestCountingEphemerisEngine::attachCatalogBodies(EphemerisSnapshot& snapshot) const
{
    if (m_catalogBodies != nullptr) {
        snapshot.catalogBodies = m_catalogBodies;
    }
}

EphemerisEngineOptions highPrecisionLightTimeOptions() noexcept
{
    EphemerisEngineOptions options;
    options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
    options.setCorrectionFlags(EphemerisCorrectionFlags::lightTime());
    return options;
}

EphemerisEngineKind::Type NonEnumeratedHighPrecisionTraitsEngine::kind() const noexcept
{
    return static_cast<EphemerisEngineKind::Type>(42);
}

std::string_view NonEnumeratedHighPrecisionTraitsEngine::name() const noexcept
{
    return "Non-enumerated high-precision traits test engine";
}

EphemerisEngineTraits NonEnumeratedHighPrecisionTraitsEngine::traits() const noexcept
{
    return EphemerisEngineTraits::highPrecisionEngine();
}

EphemerisEngineOptions NonEnumeratedHighPrecisionTraitsEngine::options() const noexcept
{
    EphemerisEngineOptions options;
    options.setEngineKind(kind());
    return options;
}

EphemerisSnapshot NonEnumeratedHighPrecisionTraitsEngine::compute(const EphemerisRequest& request) const
{
    EphemerisSnapshot snapshot;
    snapshot.context = request.context;
    return snapshot;
}

std::optional<CelestialBodyState>
NonEnumeratedHighPrecisionTraitsEngine::computeBodyState(const EphemerisRequest&, const std::string_view) const
{
    return std::nullopt;
}

std::optional<CelestialBodyState>
NonEnumeratedHighPrecisionTraitsEngine::computeBodyState(const EphemerisRequest&, const std::size_t) const
{
    return std::nullopt;
}

EphemerisSnapshot
NonEnumeratedHighPrecisionTraitsEngine::compute(const skygate::core::ObservationContext& context) const
{
    EphemerisSnapshot snapshot;
    snapshot.context = context;
    return snapshot;
}

std::optional<CelestialBodyState> NonEnumeratedHighPrecisionTraitsEngine::computeBodyState(
    const skygate::core::ObservationContext&, const std::string_view
) const
{
    return std::nullopt;
}

std::optional<CelestialBodyState> NonEnumeratedHighPrecisionTraitsEngine::computeBodyState(
    const skygate::core::ObservationContext&, const std::size_t
) const
{
    return std::nullopt;
}

}  // namespace skygate::ephemeris::tests
