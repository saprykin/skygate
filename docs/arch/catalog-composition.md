# Catalog composition

How catalog payloads become bodies, and how bodies from several sources
merge into one catalog. Parsing and composition live in
`libs/skygate-ephemeris`.

## Schemas and containers

Catalog ingestion separates payload containers from payload schemas.

A schema defines how a decoded plain-text payload maps to catalog bodies.
`CatalogSchemaRegistry` is the single source of truth: a
`CatalogSchemaDescriptor` records the schema type, diagnostic name,
delimiter, exact required columns, and parser factory for each registered
schema. Built-in schemas are the bundled starter catalog, HYG CSV
(comma-separated, required `ra`/`dec`/`mag`), and OpenNGC CSV
(semicolon-separated, required `Name`/`Type`/`RA`/`Dec`). Header detection,
parser selection, and parse diagnostics all read the same descriptors.

A container is an encoding layer around any schema. `CatalogPayloadParser`
decodes at most one gzip or ZIP layer before schema dispatch, then detects
the schema of the decoded payload. Nested archives are reported as an
unsupported inner schema instead of being decoded recursively. A ZIP member
can be selected explicitly through `CatalogParseRequest::memberSelector`;
otherwise the unique supported member is chosen deterministically and
missing or ambiguous members are explicit errors. Container codecs
(`CompressedDataInflater` and the private `io/zip` layers) are shared by
every schema rather than owned by one source.

Parse options travel as one value. `CatalogParseRequest` carries the
payload, an optional archive member selector, and an optional schema hint.
Detection stays authoritative: a detected schema that differs from a
concrete hint fails with `CatalogLoadResult::ErrorCode::SchemaHintMismatch`
instead of silently parsing something else. `CatalogSelectionOptions` can
truncate to the brightest N bodies after parsing.

The public catalog API has only narrow front doors:

- `CatalogPayloadParser` for unknown downloaded/imported payloads
  (container decoding, detection, and parsing).
- `CatalogLoader::load(...)` for a known schema plus an already-decoded
  payload, with diagnostics.
- `CatalogFactory::createStarCatalogFromBodies(...)` and
  `createBundledStarCatalog()` for test/UI fixtures, already parsed bodies,
  and the bundled starter dataset.
- `CatalogSchemaRegistry::registerSchema(...)` to add a schema.
- `CatalogComposer::composeCollection(...)` for ordered source collection
  composition; application composition lives in `SkyCatalogRuntime`.

Parsers implement `ICatalogParser` and return a `CatalogBodyParseResult`;
schema-specific field mapping stays local (`HygCatalogParser`,
`OpenNgcObjectMapper`, `BundledCatalogParser`). Parsed own-galaxy bodies
are normalized by `CatalogBodyNormalization`; distant deep-sky bodies keep
parser-provided metadata. `CatalogSnapshotValidator` enforces the snapshot
invariants at construction boundaries, and the final catalog is
materialized as an `InMemoryStarCatalog`.

Catalog parsing uses private helpers to avoid repeated policy fragments:
`StringUtilities` covers small ASCII normalization helpers,
`CatalogParsingUtilities` covers catalog field parsing, and
`OpenNgcObjectMapper` owns OpenNGC alias/id/display-name/object-kind
mapping. ZIP handling is split into private layers: `ZipDirectoryReader`
parses central-directory metadata, `CatalogZipEntrySelector` applies the
archive member choice policy, and `ZipEntryExtractor` validates local
headers and inflates entry payloads.

Private catalog implementation files are grouped by responsibility under
`libs/skygate-ephemeris/src/catalog`: orchestration facades and
identity-based composition at the catalog root, the bundled core-body
augmentation helper in `composition/`, normalization in `normalize/`,
source-specific parsers in `bundled/`, `hyg/`, `opengc/`, and
`stellarium/`, constellation codecs in `constellation/`, and
payload/archive IO in `io/` and `io/zip/`. Shared string helpers live
directly under `libs/skygate-ephemeris/src`.

## Object identity and merge precedence

Identity and display metadata stay separate.

`BaseCelestialBody::id` is the canonical object identity used by rendering,
search, ephemeris, and references. It comes from the highest-priority
source identifier at parse time (for example `hip_<n>`, `hyg_<n>`,
`ngc_<n>`, `messier_<nnn>`), never from a display name or sky position.
`CatalogObjectIdentity` preserves the source facts separately: the raw
source record key (`sourceRecordId`), namespaced cross-identifiers
(`CatalogIdentifier` with per-namespace normalization), display aliases,
and the scope of the canonical ID.

`IdScope::Global` marks a recognized astronomical designation or domain
identity (including planet IDs such as `mercury`). `IdScope::SourceLocal`
marks a parser-generated record key such as `hyg_auto_<n>` for a row
without any recognized designation. Composition qualifies a source-local
ID with its owning instance ID (`hyg_auto_1@<sourceId>`), so equal
counters from unrelated sources never match while a reload of the same
instance keeps the same object key.

`CatalogIdentityIndex` resolves canonical IDs, external identifiers, and
deep-sky aliases against the current survivors while the merge proceeds. It
prefers authoritative keys (canonical ID, external identifier) over
aliases and reports ambiguous matches explicitly.

`CatalogCompositionMerger` is the single merge implementation:

- A collection composes in order. Within one source, the first record for
  an identity is authoritative and later duplicates fill its missing
  metadata. Across replacing sources, a later source has higher precedence:
  its matching record replaces the earlier survivor and absorbs the
  earlier body's non-conflicting identifiers, aliases, and metadata. The
  winner's own present values stay authoritative; a value it is missing is
  taken from the highest-precedence source that actually supplied it, even
  when that source reached the survivor through an intermediate survivor.
  The absorbed body's canonical id, and every canonical id it already
  retained, stay on the survivor as retained canonical ids: later rows,
  later compositions, restored snapshots, and a snapshot fed back as a
  composition input still resolve the earlier identity to the single
  survivor. The public canonical id stays the surviving record's own id:
  the replacing record's for a replacement, the bridging record's for a
  bridge. That choice never changes which row's metadata outranks which.
- Enriched fields resolve per field from the source that actually supplied
  the value. A field inherited through an intermediate survivor keeps the
  rank of its supplying source instead of being promoted to the survivor's
  rank, so an explicit higher-precedence value stays authoritative while
  an absent field and a valid zero value keep their meaning, and separate
  optional fields resolve independently. Within one source, each field
  keeps the earliest row that supplied it, so a bridge across a source's
  own survivors fills only missing fields and never demotes values the
  earlier rows supplied. When the collection pass resolves a record of the
  source being merged to a survivor its own earlier rows produced, it
  fills missing values, keeps values the survivor inherited from other
  sources, and resolves a value both records supply from this source by
  the earlier supplying row, even when bridge absorption moved the
  survivor away from its original row order.
- Field origins are per merge run and are not carried by a body. An
  already-materialized snapshot supplied as one new source counts every
  value it holds as supplied by that source, so no cross-snapshot field
  history is promised or recovered.
- The identity-to-survivor index stays current: a replacement vacates the
  earlier survivor's keys, and identifiers acquired by a metadata union
  are registered before the next record resolves. Later records can
  therefore match identifiers an earlier record acquired in the same
  source pass, and a record that bridges several survivors of one kind
  collapses them into a single survivor with complete contributors.
  Identity bridging decides which records are one object, not which row's
  metadata wins, so the earlier rows keep their precedence under the
  within-source row order. Matched survivors of an incompatible kind stay
  distinct with a diagnostic.
- Weak deep-sky alias matches merge only when neither record carries a
  conflicting authoritative designation; two distinct recognized
  designations that share a common display name stay distinct.
- Coordinates merge as one coherent model per survivor, selected before
  any compatibility check. The model anchor is the fixed position together
  with the astrometry reference position and reference epoch; astrometry
  without motion or parallax fields still anchors a model. The anchor
  origin is the higher-precedence origin among the model components the
  survivor carries. A losing model whose anchor origin outranks the
  winner's replaces the winner's whole model, so copying the record that
  bridges two objects never preselects a model the earlier supplying row
  outranks. The replaced model's compatible astrometry still enriches the
  selected model field by field, including when a fixed-only model
  replaces a bridge's astrometry: the selected fixed position stays the
  model's anchor and the adopted fields keep the origins of the records
  that supplied them instead of moving to the selected anchor's row. A
  contradicting replaced position or astrometry is diagnosed, never
  combined with it, and a compatible replaced fixed position is dropped
  with the rest of the replaced model. A donor whose own fixed position
  contradicts its own astrometry contributes only its fixed position, and
  the discarded astrometry is rejected with a diagnostic. With the
  winner's model selected, a losing fixed position fills a missing winner
  position or replaces one when the losing position's origin outranks the
  winner's (a higher source rank, or an earlier supplying row within the
  same source), and never replaces a value the winner carries from a
  higher-precedence source. It never enters a model whose astrometry
  anchors the object at a different direction, because a fixed position
  carries no reference epoch to convert; a conflicting position the winner
  carries is diagnosed and replaced. A compatible losing astrometry enters
  only when it agrees with the surviving model, either directly against a
  fixed position or against the winning reference position after its own
  proper motion accounts for the reference-epoch difference; it fills
  missing proper motion, parallax, radial velocity, and validity fields,
  and it replaces a field when the losing value's origin outranks the
  winner's (a higher source rank, or an earlier supplying row within the
  same source), and never replaces a value the winner carries from a
  higher-precedence source. Contradictory losing coordinates are rejected
  with a diagnostic that names the kept model.
- After the merge, an authoritative identity shared by two active
  survivors of the same kind is an internal merge error; a shared identity
  across incompatible kinds is a deliberate, already diagnosed conflict.

`CatalogCompositionResult` reports provenance for the composition:
`sourceIds` is parallel to the composed snapshot and names the winning
source instance per body, and, for each body, `contributorSourceIds` lists
every contributing source in descending source precedence: the winner's
source first, then each remaining contributor highest precedence first,
every contributing source once. Accumulator insertion positions after a
replacement or bridge carry no precedence; within one source, the first row
is authoritative and later rows only fill it. `CatalogCompositionRequest`
carries the ordered `CatalogCompositionSourceEntry` values (`sourceId`,
`enabled`, catalog, `CatalogCompositionPolicy`); the composer rejects an
empty or duplicate source identity with an explicit diagnostic instead of
conflating provenance.

## Composition policy and fallback

Policy is part of the configuration, not a hidden merge step.

- `CatalogCompositionPolicy::Merge` and `DeepSkyOnly` are replacing
  sources: later sources win and absorb non-conflicting metadata.
- Every policy shares one identity decision. A gap-fill body is skipped
  only when its identity resolves to an existing survivor through the same
  kind, designation, and ambiguity checks `Merge` uses; a contradicted or
  ambiguous weak alias match keeps the body as an independent object with
  the normal diagnostic, and a fallback row that matches a configured
  survivor preserves the configured winner's values.
- `AugmentCore` contributes only non-deep-sky bodies as a gap-fill; it
  also enables the bundled bright-star fallback when no source supplies a
  star. The runtime always adds it as a derived contribution so Sun, Moon,
  and planet bodies exist; it is not a configured collection source.
- `DeepSkyFallback` contributes only deep-sky identities that no
  configured source supplies. It never replaces a configured source's body
  or its visible collection position, so configured values always win over
  bundled fallback data.
- Bundled augmentation and fallback carry their own provenance
  (`bundled-core`, `bundled-deep-sky`) instead of being attributed to
  another source.

`SkyCatalogRuntimeBuildOptions::BundledDeepSkyParticipation` makes bundled
deep-sky participation an explicit configuration choice (`Disabled` or
`Fallback`); legacy preset controls delegate to the same configuration.
The bundled fallback cannot alter an explicit source's values (a configured
M31 with magnitude 0 keeps that magnitude).

Deep-sky objects remain a fixed-equatorial catalog layer. The UI can use
bundled Messier data or download/update the OpenNGC preset. OpenNGC records
are parsed and deduplicated in `libs/skygate-ephemeris`, not in QML or
scene graph code.

## Constellation data

Constellation lines and label anchors have one persisted/imported source:
the optional downloaded Stellarium skyculture data, parsed by
`StellariumConstellationParser`.

`SkyCatalogRuntime` keeps the dataset produced by a source instance's
related download owned by that instance, and `SkyCatalogCacheController`
persists it with the instance. When Stellarium constellation data is
unavailable or cannot be parsed, the app keeps constellation refs empty
rather than rendering hand-authored bundled outlines. The active view is
composed from the owned datasets of the enabled sources by
`SkyCatalogRuntime`, and `ConstellationReferenceResolver` maps the
dataset's HIP-based references onto the surviving objects of the active
snapshot.

`StellariumConstellationParser` is a small orchestration entrypoint over
private helpers: `StellariumHipParser`, `StellariumLineRefExtractor`, and
`StellariumAnchorGroupExtractor`.
