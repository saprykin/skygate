# skygate-ui module

`skygate-ui` is the application layer and the most stateful part of the
system. `apps/skygate-ui/src/main.cpp` stays with the executable target. The
`skygate-ui-lib` sources under `apps/skygate-ui/src` are grouped by
subsystem: `app`, `catalog`, `diagnostics`, `ephemeris`, `location`,
`platform`, `render`, `scene`, `search`, `selection`, `settings`, `theme`,
and `time`.

## Presentation objects

### SkyContextController

Primary QML-facing controller (`QObject` with `Q_PROPERTY` and `Q_INVOKABLE`
API).

- Owns the current `skygate::core::ObservationContext`, projection
  selection, view center and field of view, playback state, timeline
  settings, location source selection, and location status.
- Owns `SkySettingsStore`, `SkyCatalogManager`, `SkyEphemerisDataManager`,
  and the bundled city catalog model used by Preferences.
- Owns the selected `IEphemerisEngine` and recreates it when catalog,
  engine-option, or ephemeris-data state changes.
- Owns `SkyTimeController`, the QML-facing date/time surface for display
  timezone selection, civil-time formatting, and fixed-time entry.
- Uses a `QTimer` for live timeline updates.
- Requests a one-shot observer location update through Qt Positioning when
  the persisted location source is `Current Device`.
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

### SkyViewportItem

Custom `QQuickItem` that draws the sky in the Qt scene graph. It renders
projected celestial points, symbolic deep-sky glyphs, projected
constellation segments, the horizon line, the altitude/azimuth grid, and
highlighted cardinal azimuth lines. A mutex-protected shared render-data
snapshot bridges scene-model data into `updatePaintNode()`.

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

Persists and restores the collection configuration, per-instance payload
and binary sidecars, and per-source related datasets. It migrates the
legacy two-slot cache once, and only for a true first-use outcome (no
collection was ever committed); neither an unreadable committed collection
nor a readable older flat collection is replaced by retired legacy data.

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
Stages and atomically activates verified ephemeris data updates. Emits
data-revision changes that invalidate the selected engine and scene caches.

### CatalogCoordinator

Orchestrates the download and parse workflow. Separates transport concerns
from parse concerns.

### CatalogDownloadService

Tries multiple URLs until the first successful download. Applies request
headers, timeout, and maximum payload size checks.

### CatalogPayloadParseService

Parses downloaded payloads on `QThreadPool::globalInstance()`. Marshals
progress and completion callbacks back onto the UI thread.

### SkySettingsStore

Persists controller state in `QSettings`:

- location source and selected city id alongside raw observer coordinates,
  so device/city/custom modes survive relaunch;
- the selected display timezone as an IANA timezone id; UTC stays the
  internal calculation and storage time basis;
- the catalog collection configuration (instance identity, order, policy,
  enabled state, parse options, attribution, bundled flag) and per-instance
  payload/binary sidecars in the app-data cache directory. See
  [Catalog collection lifecycle](catalog-lifecycle.md) for the storage and
  recovery semantics.

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
