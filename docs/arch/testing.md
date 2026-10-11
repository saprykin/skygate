# Testing

Tests stay close to each module.

## libs/skygate-core/tests

Angle/projection math, viewport math, type validation, projection factory
behavior, prepared projections, and concrete projection strategies.

## libs/skygate-ephemeris/tests

Coverage includes schema registration and detection, payload parsing
helpers, HYG/OpenNGC parser edge cases, container (gzip/ZIP) decoding and
member selection, constellation parsing, catalog factory behavior, binary
codec round trips, celestial reference calculations, engine baselines and
fallbacks, and fixed-date ephemeris regression checks.

Catalog merge regressions are owned by the invariant they protect:

- `CatalogIdentityMatchTests` covers matching and ambiguity.
- `CatalogBridgeRetentionTests` covers bridge identity and provenance
  retention.
- `CatalogSourcePrecedenceTests` covers source precedence after
  replacements and bridges.
- `CatalogRowOriginTests` covers earliest-row values across same-source
  and repeated bridges.
- `CatalogMagnitudePrecedenceTests` covers magnitude rules with absent,
  zero, and negative values.
- `CatalogModelSelectionTests` with `CatalogModelReplacementTests` covers
  coherent coordinate-model selection and later replacements.
- `CatalogPerformanceGuardTests` holds the large-fixture merge guard.

Events are split by contract: `ObservationEventDirectSearchTests` for
direct search requests, `ObservationEventGuidanceTests` for guidance
strategies, `ObservationEventPrimaryEngineTests` for primary-engine
verification.

High-precision coverage: `HighPrecisionEngineDispatchTests` for dispatch
and batching, `HighPrecisionEngineCacheTests` for cache identity and
concurrency, `HighPrecisionResultBuilderTests` for default result-builder
metadata. Apparent-place transforms split into correction routing
(`ApparentPlaceCorrectionRoutingTests`), topocentric and refraction
failure behavior (`ApparentPlaceTopocentricTests`), and Horizons fixture
validation (`ApparentPlaceHorizonsValidationTests`).

Factory and request contracts live in `EphemerisEngineFactoryModelTests`,
`EphemerisEngineFactorySelectionTests`,
`EphemerisEngineFactoryResultTests`, and
`EphemerisRequestTimeContractTests`.

## apps/skygate-ui/tests

Grouped by matching UI responsibility folders plus shared `support`
fixtures: scene-model behavior, controller/search/location/theme/overlay
models, settings persistence, and QML load/interaction/rendering smoke
coverage.

Catalog manager lifecycle:

- `SkyCatalogManagerSourceTests` covers presets, source CRUD, ordering,
  enabled state, and first-use defaults.
- `SkyCatalogManagerImportTests` covers import completion, cancellation,
  failures, and accepted-update facts.
- `SkyCatalogManagerRelatedReplyTests` covers reply lifetime, stale
  completions, and owner independence.
- `SkyCatalogManagerRelatedReplacementTests` covers accepted, rejected,
  and rolled-back related-declaration replacement and publication
  alignment.
- `SkyCatalogManagerRelatedPersistenceTests` covers owned related-payload
  persistence, eviction, migration, and restart recovery.
- `SkyCatalogManagerPersistenceTests` covers routine configured-source
  and cache round trips and restart restoration.
- `SkyCatalogManagerLiveRestoreTests` covers active-operation lifetime
  while an already running manager restores.
- `SkyCatalogManagerRecoveryTests` covers legacy-ID migration,
  interrupted-generation upgrades, and unreadable committed-collection
  recovery.
- `SkyCatalogManagerCompositionTests` covers combined composition and
  interchangeability workflows.

Collection cache behavior splits between `SkyCatalogCacheRestoreTests`
(restoration), `SkyCatalogCacheRelatedDataTests` (source-owned related
data), and `SkyCatalogCacheMigrationTests` (legacy migration with
committed-collection recovery). Runtime publication splits between
`SkyCatalogRuntimeTransitionTests` (source transitions and atomic
publication), `SkyCatalogRuntimeRelatedDataTests` (related-data
ownership), and `SkyCatalogRuntimePresentationTests` (bundled
fallback/provenance/status presentation).

Settings and cache: `SkySettingsStoreTests` covers ordinary settings,
defaults, malformed values, and logging preferences.
`SkyCatalogCacheSettingsTests`, `SkyCollectionGenerationTests`, and
`SkyEphemerisCacheSettingsTests` cover catalog cache
serialization/eviction, collection generation publication/recovery, and
ephemeris cache metadata.

Ephemeris data manager phases are owned by `SkyEphemerisDataStatusTests`
(status and installed-data selection), `SkyEphemerisDownloadTests`
(download, staging, and security checks), and
`SkyEphemerisActivationTests` (activation, rollback, and cancellation),
with controller integration in `SkyContextControllerEphemerisDataTests`.
QML preferences separate catalog controls (`QmlPreferencesCatalogTests`)
from ephemeris engine and data controls (`QmlPreferencesEphemerisTests`).

Trails and performance: `SkyObjectTrailGeometryTests` covers geometry and
clipping, `SkyObjectTrailGuidanceTests` covers guidance and adaptive
sampling, `PerformanceSceneProfileTests` covers scene and selection
profiling, and `PerformanceSearchGuardTests` covers search, hit-test, and
trail guardrails.

Fake-network catalog download and coordinator workflows stay in
`CatalogCoordinatorDownloadTests`, `CatalogDownloadServiceTests`, and
`SkyCatalogImportWorkflowTests`.

Network-facing catalog tests use deterministic fake `QNetworkAccessManager`
responses instead of external services, and the QML smoke test checks
load/registration health without asserting visual rendering.
Rendering-independent logic stays testable without a running UI.
