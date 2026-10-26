# Extension points

The architecture grows by adding new implementations behind existing
seams.

## Adding a projection

Update:

- `ProjectionType` with the new enum value
- `ProjectionTypeCodec` with its ordered value, display text, and parse
  entry. The preferences view reads `projectionTypeOptions` from the
  controller, which lists these values, so the QML side needs no change.
- `ProjectionAlgorithms` with the frame setup in `prepareFrame` and the
  formula in `project`
- projection tests under `libs/skygate-core/tests/projection`

## Adding a catalog source that uses an existing schema

A source with a registered schema needs no new parser, renderer branch, or
scene-graph change.

1. Add a `SkyCatalogSourceDescriptor` entry in
   `apps/skygate-ui/src/catalog/SkyCatalogPresets.cpp`: a stable
   `sourceId`, title, optional version, URLs for a downloaded source,
   the `schemaHint`, optional `archiveSelector`, optional
   `relatedDatasetUrls`, attribution, category, and, for bundled data,
   the bundled flag. `SkyCatalogSourcePresetModel` exposes a non-bundled
   descriptor to QML automatically.
2. A bundled source reconstructs its objects through `CatalogFactory`
   instead of URLs; a downloaded source needs no factory change when its
   schema already exists.
3. Keep schema-specific field mapping inside the existing schema mapper
   and declare any new cross-identifiers through `CatalogIdentifier`. A
   source whose payload describes the same objects as an existing schema
   does not need a new identity namespace.
4. Add descriptor identity and metadata tests in
   `apps/skygate-ui/tests/catalog/SkyCatalogSourceDescriptorTests.cpp`,
   plus a deterministic fake-download scenario in the focused catalog
   manager suite that owns the affected behavior (see
   [Testing](testing.md)). Use the existing fake network manager and local
   fixtures.

Consumers stay generic: rendering, search, inspector, ephemeris, and
constellation resolution read `IStarCatalog::bodies()`, common body-kind
metadata, `SkyCatalogManager::sourceIds()`, and `contributorSourceIds()`;
they must not branch on the source's schema or title. Body kind alone
supplies neither motion nor shape: those come from the shared optional
domain metadata (star astrometry, deep-sky axes and classification), which
consumers read uniformly instead of special-casing a schema.

## Adding a schema

A new schema needs a payload mapper and one registry registration.

1. Add the schema value to
   `libs/skygate-ephemeris/src/catalog/CatalogSourceType.hpp`.
2. Implement `ICatalogParser`
   (`libs/skygate-ephemeris/src/catalog/ICatalogParser.hpp`) in a focused
   directory under `libs/skygate-ephemeris/src/catalog` (for example
   `bundled/`, `hyg/`, or `opengc/`), returning a
   `CatalogBodyParseResult`. Reuse `DelimitedCatalogReader` and
   `CatalogParsingUtilities` for delimited data, and declare
   cross-identifiers through `CatalogIdentifier`.
3. Register a `CatalogSchemaDescriptor` in `CatalogSchemaRegistry.cpp`:
   delimiter, exact required columns, diagnostic name, and parser factory.
   Detection, `CatalogLoader`, and parse diagnostics pick it up from
   there. `CatalogSchemaRegistry::registerSchema` is for test adapters and
   later registrations.
4. Reuse the shared container codecs (`CatalogPayloadParser`,
   `CompressedDataInflater`, and the private `io/zip` layers) instead of
   adding an archive decoder.
5. Add tests through the per-module CMake helper in
   `libs/skygate-ephemeris/tests/CMakeLists.txt`:
   `CatalogSchemaRegistryTests.cpp` for detection and parser selection, a
   parser test for field mapping, `CatalogContainerDecodingTests.cpp` for
   gzip/ZIP variants of the same payload, and the
   `CatalogSemanticFixtureAdapter` corpus in
   `CatalogInterchangeabilityTests.cpp` so equivalent objects stay
   equivalent across schemas.

## Catalog extension limits

- Catalogs are eager, immutable snapshots (`IStarCatalog`). Lazy, paged,
  or remote storage that cannot materialize the complete snapshot is
  outside the current contract; such a source needs a new provider
  contract, not another schema.
- A new object kind or motion model is domain and engine work. A catalog
  can supply the data, but rendering and ephemeris consume the shared
  kind/metadata interfaces, so those consumers need to understand the kind
  as well.

## Replacing or adding an ephemeris engine

Provide another `IEphemerisEngine` implementation and register it in the
central descriptors instead of teaching every consumer its name.

- `EphemerisEngineDescriptor` records a stable string id, an
  `EphemerisEngineKind`, a display name, default `EphemerisEngineOptions`,
  and whether the engine supports correction and atmosphere settings.
  `EphemerisEngineDescriptorRegistry` is the static table that maps ids,
  kinds, and UI indices to descriptors. Settings persist the stable id
  (for example `"simple"` or `"highPrecision"`) rather than a UI position,
  so adding an engine does not disturb existing stored selections.
- `EphemerisEngineTraits` records which behaviors an engine supports:
  scene-correction policy support, inspector detail recomputation, guided
  approximate event search, adaptive trail sampling, and live-recompute
  throttling. `EphemerisComputationPolicy` resolves those questions at one
  boundary from the active engine's traits and the request, so consumers
  branch on policy instead of identity.
- Event and trail sampling obtain their inexpensive guidance engine from
  an explicit `IEphemerisGuidanceStrategy` supplied during composition
  (`SimpleEphemerisGuidanceStrategy` currently provides the simple
  engine). See [skygate-ephemeris module](ephemeris-engine.md) for the
  fallback and engine-identity contracts.
- `EphemerisBackendResourceCache` owns the time-scale, Earth-orientation,
  and CALCEPH kernel providers, invalidates them on data revision or
  manifest changes, and copies them into factory requests.
  `EphemerisEngineReplacementPolicy` states when a degraded rebuild may
  not replace the active engine.

New implementations add descriptor entries, define their traits and
defaults, wire the factory switch, and pass the contract and architecture
checks: `EphemerisEngineContractTests` runs the shared interface contract
against every implementation, `EphemerisRequestTimeContractTests` checks
epoch authority, and the engine header isolation test forbids concrete
engine includes outside the factory, composition component,
implementation directories, and test support.

## Evolving the rendering path

The scene model already isolates render preparation from drawing.
Alternate renderers can reuse `SkySceneModel`, `PreparedProjection`, and
the snapshot types while changing only the final presentation adapter.
