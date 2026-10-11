# Catalog collection lifecycle

How configured catalog sources become an active catalog, and how the
collection survives restart. The classes involved are `SkyCatalogManager`
(lifecycle owner), `SkyCatalogRuntime` (active composition), and
`SkyCatalogCacheController` (persistence and migration).

## Source identity

`SkyCatalogSourceDescriptor` is a preset template; `SkyCatalogSourceInstance`
is a configured instance. The instance ID is allocated when the instance is
added (`SkyCatalogSourceInstance::allocateInstanceId`) and stays fixed
through reload, reorder, cache, provenance, and restart. `descriptorId`
records the template an instance came from; version and archive selection
never enter the ID, so two instances of one descriptor and two versions or
member selections at one URL stay distinct.

## First use versus configured emptiness

The bundled default source is installed only while no saved configuration
has been considered yet. Once a configuration is installed, removing the
sole source or replacing the collection with an empty one stays empty
through every rebuild; no later rebuild silently recreates a configured
source.

## Atomic activation

A source mutation or rebuild composes the candidate configuration and every
derived value before publishing. Configured sources, active snapshot,
counts, provenance, related view, and revision change together, and only
for an accepted composition. A rejected transition (null catalog,
composition rejection, missing bundled catalog) reports the operation
error and keeps the last accepted state; a deliberate empty result is a
published state, not a half-applied failure.

## Related data

Constellation lines, anchor groups, and the declared count belong to the
source instance whose related download produced them
(`SkyCatalogSourceRecord::constellationData`). Disabling a source keeps its
data owned but inactive; removing the source removes the data. The active
view composes the owned datasets of the enabled sources in visible
collection order:

- line segments keep visible order; an exact duplicate segment from a
  later contributor is kept once
- anchor groups are keyed by constellation name; a later contributor's
  definition replaces an earlier one, matching later-wins object
  precedence
- the declared count comes from the last enabled contributor that declares
  one and is never lower than the number of distinct names in the view

References resolve through `ConstellationReferenceResolver` against the
active snapshot, so they follow the surviving object identity even when
the winning star came from another catalog.

An accepted activation applies the old and new related-data declaration
to the instance's own dataset before the transition is persisted or
published. A declaration that still selects related data supersedes the
dataset owned by the replaced catalog until the replacement's own download
completes; a declaration that no longer selects related data retires the
obsolete dataset. Downloaded and bundled activation paths share this
rule, independent of composition policy and payload origin, and no other
instance's dataset is touched. A rejected update keeps the previously
accepted dataset.

## Configuration versus payload cache

Durable configuration and disposable payload data are separate stores.
Clearing one source's cache evicts only its disposable payloads (catalog
bytes, binary cache, and its owned related payload); the configured record
(identity, order, enabled state, descriptor, parse options, and URLs)
survives. The accepted in-memory snapshot stays active for the session.
After a restart the source is still configured and reports an unavailable
payload with retry state, and ordinary persistence never writes the
evicted bytes back. Only a later accepted load repopulates payloads;
Remove stays the only configuration-deletion operation. A configured
source whose payload is missing or corrupt likewise stays configured and
reports its own error without erasing siblings.

## Persistence and recovery

The collection configuration (instance identity, order, policy, enabled
state, parse options, attribution, bundled flag) is stored in `QSettings`
as a versioned snapshot. An empty snapshot is still written, so an
intentionally empty collection stays distinct from "never configured".

A collection save stages a complete new generation before committing it:
generation-qualified per-instance raw and versioned `CatalogBinaryCodec`
sidecar files in the app-data cache directory, plus a generation-scoped
record set in `QSettings`. A small manifest, written last, names the
committed generation. Until that manifest is published the previous
generation stays active, so a failure while staging any payload file,
while storing records, or while publishing the manifest leaves the last
committed collection loadable instead of pairing old records with new
payloads. Removing the superseded generation afterwards is best effort
because the manifest alone decides which generation is committed.

Restores read the manifest-named generation, prefer each source's binary
snapshot, and fall back to parsing the raw payload with the stored parse
options; an unreadable or outdated sidecar degrades one source instead of
failing the collection. A named generation whose records are absent is not
treated as an empty collection.

Loading the collection returns one of three explicit states:

- no committed collection
- a loaded collection (an empty one is valid)
- unusable committed configuration (missing or truncated manifest, or a
  missing referenced generation), with diagnostics

A durable committed-generation record proves a collection was committed
even when the manifest is gone, and a staged-but-uncommitted generation is
never chosen as the current collection. A collection already committed in
the older flat format is Loaded on its own terms: while its version marker
and source records are intact, generation groups staged by an interrupted
upgrade are ignored with a diagnostic, their records and payloads are
never promoted, and the flat records load with their versions, order, IDs,
and options. Only when generation records actually replaced the flat
records does the stale marker make the committed configuration unusable.

Related constellation datasets are stored per owning source and restored
before the active related view is composed.

## Live restore

A successful restore replaces the runtime collection and the
operation/accepted-facts state atomically. Operations omitted from the
restored collection are dropped, and outstanding callbacks are superseded
by fresh revisions, including when a restored source reuses an existing
instance ID: a late reply can neither repopulate an omitted source nor
mutate a restored instance. A rejected restore preserves the previous
accepted collection and still-valid work.

## Legacy migration

Existing installations migrate the legacy two-slot cache once, and only
while no collection has ever been committed. A collection committed in the
older flat format counts, even when no generation has committed, so an
interrupted first upgrade that staged generation records without a
manifest stays on the flat collection instead of migrating. Legacy
instance IDs are derived deterministically exactly at that boundary, and a
legacy collection-wide related payload establishes an owner only when a
single record makes that unambiguous. Migration is acknowledged only after
the new configuration is committed, so an interrupted migration keeps the
legacy data readable and an intentionally emptied collection does not
resurrect migrated sources.

Legacy migration runs only for the no-committed-collection state; a
missing or truncated manifest or a missing referenced generation reports a
rejected restore instead of being treated as first use. A deliberately
evicted per-source payload stays a readable configured record that
reports unavailable payload state, distinctly from unreadable committed
data.
