# skygate-ui module

`skygate-ui` is the application layer and the most stateful part of the
system. `apps/skygate-ui/src/main.cpp` stays with the executable target. The
`skygate-ui-lib` sources under `apps/skygate-ui/src` are grouped by
subsystem: `app`, `catalog`, `diagnostics`, `ephemeris`, `location`,
`platform`, `render`, `scene`, `search`, `selection`, `settings`, `theme`,
and `time`.

The library exposes `apps/skygate-ui/src` as its only include root. A file
that reaches into another subsystem includes its header by path from that
root, for example `scene/SkySceneModel.hpp`; headers in the same directory
are included by name. Test targets work from the `tests` root in the same
way: support headers appear as `support/...`, and UI headers keep their
paths from the `src` root.

## Presentation objects

### SkyContextController

Primary QML-facing controller (`QObject` with `Q_PROPERTY` and `Q_INVOKABLE`
API).

- Owns the current `skygate::core::ObservationContext`, projection
  selection, view center and field of view, playback state, timeline
  settings, location source selection, and location status.
- Owns `SkySettingsStore`, `SkyCatalogManager`, `SkyEphemerisDataManager`,
  and the bundled city catalog model used by Preferences.
- Forwards the ephemeris data status and update surface to
  `SkyEphemerisUpdateWorkflow`, and keeps the persisted data profile
  selection in user settings.
- Keeps the QML engine settings surface and engine status text, and
  delegates engine creation, replacement, backend-resource composition, and
  request construction to `SkyEphemerisSession`.
- Owns `SkyTimeController`, the QML-facing date/time surface for display
  timezone selection, civil-time formatting, and fixed-time entry.
- Uses a `QTimer` for live timeline updates.
- Delegates the one-shot observer location update to
  `SkyDeviceLocationAcquisition` when the persisted location source is
  `Current Device`.
- Loads persisted settings and the restored catalog cache during
  initialization.

### SkySceneModel

Read-model and derived-state layer between controller state and rendering.
Listens to `SkyContextController::skyContextChanged()`. Produces:

- a cached `skygate::ephemeris::EphemerisSnapshot`
- a cached `skygate::core::PreparedProjection`
- a `SkyRenderFrame` with projected points, constellation segments,
  deep-sky glyphs, and labels
- `overlayItems` for QML overlays

It also builds a spatial hover lookup for hit-testing object labels.

`sceneFrameChanged` announces everything that can change QML overlays,
including inspector moves and pin toggles. The viewport is driven by a
separate notification: `SkySceneModel` publishes a `SkyViewportSnapshot`
holding the prepared projection, the resolved observation context from the
reference snapshot, the render palette, layer visibility, and the render
primitives. The snapshot is replaced only when the render-frame generation
or the active trail target changes, so overlay-only edits leave it in
place. The generation inside the snapshot identifies the handoff; a new
generation means the viewport has new geometry to build.

### SkyObjectTrailBuilder

Builds the selected or tracked object's trail into the render frame. It
keeps projection, clipping, dashed past segments, future tick marks, and
their labels. Sampling lives in `SkyObjectTrailSampler`, whose inputs are
the engine, target, and observation context; theme, viewport, and
projection state stay outside. The sampler picks the fixed-coordinate,
guidance, adaptive, or direct sampling path in that order and caches the
full sample window. Reusable direction math (axis rotation, angular
separation, and interpolation between directions) lives in
`skygate::core::SphericalGeometry`; the sampler keeps the sampling window,
step sizes, interpolation error threshold, and the guidance-alignment
policy.

### SkyViewportItem

Custom `QQuickItem` that draws the sky in the Qt scene graph. It renders
projected celestial points, symbolic deep-sky glyphs, projected
constellation segments, the horizon line, the altitude/azimuth grid, and
highlighted cardinal azimuth lines. It consumes the immutable
`SkyViewportSnapshot` published by `SkySceneModel`: a mutex-protected
`shared_ptr` bridges the snapshot into `updatePaintNode()`. Overlay-only
edits, such as inspector moves, pin toggles, and overlay text changes, do
not trigger viewport synchronization. `SkySceneModel` keeps the current
snapshot while the render-frame generation and the active trail target are
unchanged. `updatePaintNode()` still rebuilds the geometry whenever it runs,
including after a resize. Styling and observation data come from the
snapshot, so the item never reaches into `SkyContextController`.

Line segments are grouped into one geometry node per full RGBA color and
quantized width, so transparency stays part of a line's material identity.
Widths are quantized to hundredths of a pixel, which keeps float rounding
in computed widths from splitting one intended width across nodes. Nodes
are appended in ascending order of the legacy packed key
`((rgba & 0x00ffffff) << 8) ^ widthKey`; alpha ascending breaks ties where
the packed value collides, so previously distinct buckets keep their draw
order. Zero-length segments are dropped before they contribute geometry.

A geometry-changing snapshot rebuilds these nodes. `updatePaintNode()`
clears both roots and creates one geometry node per bucket, each with its
own `QSGGeometry` and `QSGFlatColorMaterial`. Measured with the large mixed
catalog fixture at 1280 by 800 in a debug build, a pan frame held six line
buckets and two point buckets, so one geometry change recreated eight
nodes in 24 object allocations, about 2 KB inside a paint call that spent
216 allocations and 400 KB; the scene rebuild producing the snapshot spent
about 3400 allocations. `QSGGeometry::allocate()` reallocates vertex
storage whenever the vertex count changes, so retained nodes would not
avoid vertex-buffer churn during pan or zoom. Bucket counts follow palette
colors and widths rather than scene size, so rebuilding them costs a few
percent of the paint call and node retention is not implemented.

### Render helpers

`SkyViewportItem` delegates segment building to `SkyViewportGeometry`
and scene-graph clearing and line/point synchronization to
`SkyViewportSceneGraph`, over the root node type
`SkyViewportSceneRootNode`. `SkyRenderLabelBuilder` owns label color,
bounds, viewport fitting, and the body and deep-sky label passes.

### Render policy

`SkyRenderLabelBuilder.cpp` and `SkyRenderBodyBuilder.cpp` keep their numeric
policy with the code that applies it: deep-sky label budgets come from
named field-of-view tiers, label scores from named weights, and star
decimation from a named state-count gate and screen-cell size. Both scale
from a reference viewport area tuned in the render code, independent of
the default main-window size kept by `SkySettingsStore`.
`SkyRenderStylePolicy` owns the magnitude-to-point-size formula, the body
color selection, and the constellation line color used by the render
builders.

### Selection and inspection

`SkySelectionMarkerBuilder` and `SkyObjectInspectorBuilder` consume the
frame pipeline output. `SkySceneFramePipeline` indexes body states by a
trimmed, case-folded ID from `SkySceneLookupKey`, and selection resolves
its target through the same key, so catalog spelling differences in case
do not split a lookup. `SkyObjectInspectorFormatters` owns the
inspector's display strings for object type, magnitude, coordinates,
observation events, ephemeris metadata, aliases, and the source label.

## QML composition

`apps/skygate-ui/qml/Main.qml` composes several layers:

- `SkyViewportItem`: base rendered star field and line work.
- `SkyInteractionLayer`: mouse drag pan, wheel or pinch zoom, and hover
  hit-testing.
- `SkyOverlayLayer`: text labels for objects and cardinal directions, plus
  collision avoidance with the app menu button and toolbar controls.
- `TimelineToolbar`: playback, stepping, speed, magnitude cutoff, and view
  reset controls.
- `PreferencesWindow`: observer location, projection, and catalog
  management UI. Uses `PreferencesDraft.qml` as a staged edit buffer before
  applying changes back to `SkyContextController`. Includes a source-aware
  location workflow with `Current Device`, `City`, and `Custom` modes plus
  a searchable city picker backed by the bundled catalog model.
- `StatusFooter`: current context status summary. Hosts the clickable
  selected-timezone text that opens the compact fixed-time popup.

The UI is hybrid: heavy drawing happens in C++ scene graph code, while
transient UI and chrome stay in QML.

## Catalog and settings subsystem

### SkyCatalogManager

Owner of the configured catalog source collection and its lifecycle,
surfaced to QML through `SkyContextController`: load, reload,
enable/disable, reorder, remove, clear cache, and restore. It keeps one
operation record per source instance with a monotonic operation revision,
so a response captured for a superseded incarnation can never apply to its
successor while unrelated instances keep their pending work. It delegates
active composition and related-data ownership to
`SkyCatalogRuntime` and persistence/migration to
`SkyCatalogCacheController`. Only an accepted transition persists state and
emits `catalogChanged`, which makes `SkyContextController` rebuild the
selected ephemeris engine.

### SkyCatalogRuntime

Owns the configured source records, the composed active `IStarCatalog`, the
counts and provenance that describe it, and the composed related
constellation view. It stages a transition (candidate configuration,
composition, counts, provenance, related view, revision) before publishing
it. A rejected transition reports the operation error and leaves the last
accepted state exactly as it was.

### SkyCatalogCacheController

Restores and persists the collection configuration, per-instance payload
and binary sidecars, and per-source related datasets through
`SkyCatalogCollectionStore`, and decides when the legacy two-slot cache
is migrated. Migration runs only for a true first-use outcome (no
collection was ever committed); neither an unreadable committed
collection nor a readable flat collection is replaced by retired legacy
data.

### SkyCatalogCollectionStore

Owns the durable catalog collection: the generation manifest, the
generation-scoped source records in `QSettings`, and the per-instance
payload sidecar files. A save stages a complete new generation and
publishes the manifest last, so a failed or interrupted save leaves the
committed collection loadable; a failed save discards the files and
records it staged. The store reads the committed generation, removes
superseded data after a successful commit, and evicts one source's
payload while its configured record stays durable. It keeps only the
storage protocol: validation and migration decisions stay in
`SkyCatalogCacheController`.

### SkyCatalogRelatedDataCodec

Serializes and parses constellation line rows and anchor group rows for
catalog related data. The row format comes from
`skygate::ephemeris::ConstellationDataCodec`; this type owns the
`QByteArray` boundary used by catalog persistence.

### SkyCatalogImportWorkflow

Downloads and parses one source instance through `CatalogCoordinator` using
that instance's parse options (schema hint and archive member selector).

### SkyCatalogPresets

Static descriptor table for the built-in presets (bundled, HYG v4.2,
bundled Messier, OpenNGC): stable source ID, title, version, URLs, schema
hint, archive selector, related-dataset URLs, attribution, and category.

### SkyEphemerisDataManager

Owns bundled manifest state and installed high-precision data cache state.
Reports kernel, Earth-orientation, leap-second, and Delta T status to QML.
Derives the short kernel label from the active data source and the kernel
asset, profile, and version metadata. `SkyContextController` exposes that
label as the read-only `ephemerisKernelDisplayText` property, notified with
the other ephemeris data status properties. Bundled fallback, missing
installed data, and kernel metadata that names no known family all produce
`Bundled`. Preferences binds the kernel row label to the property, so the
presentation does not depend on status wording. Stages and atomically
activates verified ephemeris data updates. Emits data-revision changes that
invalidate the selected engine and scene caches.

### SkyEphemerisUpdateWorkflow

Runs one ephemeris data update: refreshes the remote manifest, walks the
selected profile's assets, accounts for download progress, and asks
`SkyEphemerisDataManager` to stage each asset and to activate the verified
set. The workflow owns the refreshed manifest; pending transfers hold raw
asset pointers into it that do not extend its lifetime. Download
continuations hold a weak handle to the workflow, so destroying it
mid-update invalidates the guards; later completions return before they read
the manifest. The controller stores the profile in user settings
when activation succeeds and forwards the workflow's status changes to QML.

### SkyEphemerisSession

Owns the selected `IEphemerisEngine` and recreates it when the catalog,
engine options, or ephemeris data change. It holds the engine by
`shared_ptr` and replaces it under a mutex, so a handle obtained before a
replacement keeps its engine alive. The session also owns the ephemeris
backend-resource cache, the default guidance strategy, the engine-option
revision, and request construction from explicit catalog, data, and
revision inputs. The controller passes those inputs and keeps the QML
settings surface and engine status text.

### SkyDeviceLocationAcquisition

Runs one device location acquisition for the controller. It resolves the
platform `QGeoPositionInfoSource` when no source has been configured,
performs the location permission handshake, and issues one timed update.
The handshake goes through `ILocationPermissionGate`; its
`SkyLocationPermissionGate` implementation asks the running application
for precise, when-in-use permission. Signals report the acquisition
status text and each valid coordinate.

A source supplied through `setPositionSource` stays owned by the caller,
and the acquisition holds a guarded reference to it. Replacing the source
disconnects the previous one before the new source is connected, and a
null source disables the platform default. Sources created from the
platform default are children of the acquisition and are deleted when the
source is replaced. Permission completions record the source revision that
was current when they were requested: a completion from an earlier
revision is ignored, and a completion that arrives after the source
disappeared reports the device source as unavailable. The final update
dispatch checks the revision again after the `Location: Locating` status
notification, so a replacement made from that notification stops the
dispatch before either source requests an update. A source destroyed
during the same notification leaves the device source reported as
unavailable, and a later explicit start reaches the replacement source.
The controller keeps the observer location, the location source
selection, and the status text presentation: it forwards the status text
to QML and applies each reported coordinate while the selected location
source is `Current Device`.

### CatalogCoordinator

Orchestrates the download and parse workflow. Separates transport concerns
from parse concerns.

### CatalogDownloadService

Tries multiple URLs until the first successful download. Applies request
headers and a five-minute transfer timeout. The 128 MiB payload cap is
enforced while the reply is received: chunks are counted against a running
budget, the reply read buffer is bounded, and a source is aborted as soon
as a chunk would cross the cap. A declared content length over the cap
rejects the source before its body is read; missing or wrong lengths are
still checked against the bytes that arrive. A rejected source falls back
to the next candidate, while an explicit cancellation stops the chain.

### CatalogPayloadParseService

Parses downloaded payloads on `QThreadPool::globalInstance()`. Marshals
progress and completion callbacks back onto the UI thread.

### SkySettingsStore

Persists controller state in `QSettings`:

- location source and selected city id alongside raw observer coordinates,
  so device/city/custom modes survive relaunch;
- the selected display timezone as an IANA timezone id; UTC stays the
  internal calculation and storage time basis;
- the catalog collection entry points, forwarded to
  `SkyCatalogCollectionStore`: instance identity, order, policy, enabled
  state, parse options, attribution, bundled flag, and per-instance
  payload/binary sidecars in the app-data cache directory. See
  [Catalog collection lifecycle](catalog-lifecycle.md) for the storage and
  recovery semantics.

The store keeps only its persistence entry points. Its snapshot types live in
focused headers: `SkyPersistedStateSnapshot` and
`SkyEphemerisUserSettingsSnapshot` under `settings/`,
`SkyCatalogSourceCacheRecord`, `SkyCatalogCollectionCacheSnapshot`,
`SkyCatalogCollectionCacheLoadResult`, and `SkyCatalogLegacyCacheSnapshot`
under `catalog/`, and `SkyEphemerisDataCacheSnapshot` under `ephemeris/`.
Consumers include the snapshot they name; the catalog and ephemeris cache
controllers keep the store behind a pointer and include the storage header
only in their implementation files.

Primitive settings reads with fallbacks live in `SkySettingsValueCodecs`.
`SkySettingsSnapshotCodecs` owns splitting and merging the persisted state
snapshot and reading or writing it through `QSettings`.
`SkySettingsKeyBuilder` owns the `skyContext/` key prefix used by the store
and the snapshot codecs.

`SkyTimelineViewDefaultsPolicy` owns the shared numeric defaults for the
timeline step, magnitude cutoff, and view field of view. The runtime
timeline and view state, the per-domain settings snapshots, and
`SkyPersistedStateSnapshot` initialize from those constants. It also holds
the legacy view-center fallback. Atmospheric defaults come from
`EphemerisEngineOptions`. Timer, gesture, and legacy cache path values stay
with the controller or store code that applies them. The view-center
persistence fallback is separate from the runtime default: a versioned
settings file without those keys restores at the horizon facing north,
while a fresh runtime state starts at the 45/180 default view.

### QML models

- `SkyCatalogSourcePresetModel` / `SkyCatalogSourceCollectionModel`:
  read-only `QAbstractListModel` surfaces for the available preset
  descriptors and the active source collection.
  `SkyCatalogSourceCollectionModel` rows keep the durable instance ID as
  identity, so equal titles or URLs do not collapse distinct sources.
- `LocationCatalogModel`: loads a bundled CSV of major cities from Qt
  resources. Exposes a flat, filterable `QAbstractListModel` with country
  headers and city rows for the Preferences location picker.
- `TimeZoneCatalogModel`: uses Qt's timezone database to expose a searchable
  IANA timezone list for Preferences.
