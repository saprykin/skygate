## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-039
- Title: Add ephemeris user settings snapshot
- Source: Spec: Engine Selection; Settings
- Base ref: 62ff37fa5177d1f8109cad6c806331feb1498e90
- Head ref: 95062744799f39e3fb6d246e5f65dc7a0b0c9d9e

## Summary

The implementation adds an ephemeris user-settings snapshot, persists it
through the state snapshot codecs, and adds direct settings-store and codec
coverage. The direct codec path is mostly in place, but the app controller path
does not preserve several required user-facing ephemeris settings. A normal
controller save can overwrite preferred profile and update preferences with
defaults, so the task is not complete.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: Controller Drops Profile And Update Settings

Severity: MAJOR
File: `apps/skygate-ui/src/app/SkyContextControllerSettings.cpp`
Lines/functions: lines 48-56 and 115-126, `saveSettings()`,
`loadSettings()`

Problem:
`SkySettingsStore::EphemerisUserSettingsSnapshot` includes
`correctionPresetId`, `preferredDataProfileId`, `onlineUpdatesEnabled`,
`updatePresetId`, and `updateManifestUrl`, and the codecs save/load those
fields. The controller never copies those fields into its state on load and
never copies them into the snapshot on save.

Why it matters:
`saveSettings()` builds a fresh `StateSnapshot` and `saveStateSnapshot()` writes
the full ephemeris group. If settings contain a custom preferred data profile
or update manifest, a later controller save rewrites those fields with the
defaults from `EphemerisUserSettingsSnapshot`. This does not meet HP-039's
claimed acceptance criteria for preferred data profile and update settings
persistence.

Recommended fix:
Add controller-owned state, accessors, or an equivalent settings model for all
ephemeris user settings. Load every ephemeris user field from the snapshot,
save every field back, and add a controller-level load/save regression test
that proves custom profile and update settings survive a full app settings
round trip.

### Finding 2: Ephemeris Reload Does Not Notify Scene Consumers

Severity: MAJOR
File: `apps/skygate-ui/src/app/SkyContextControllerSettings.cpp`
Lines/functions: line 126, `loadSettings()`

Problem:
When persisted ephemeris settings are present, `loadSettings()` mutates the
selected engine/options and calls `rebuildEphemerisEngine()`, but it does not
emit `skyContextChanged()` or any ephemeris change signal. Other engine rebuild
paths, such as active ephemeris data changes and catalog changes, notify scene
consumers after rebuilding.

Why it matters:
`loadSettings()` is a public invokable. A runtime settings reload can change the
engine or correction options without causing the scene or dependent UI to
recompute until some unrelated context change happens. That weakens the
settings persistence path this task adds.

Recommended fix:
After applying persisted ephemeris settings, emit the same relevant change
notifications used by other rebuild paths. Add a focused controller test that
loads a different engine/options snapshot and verifies scene/context change
notification occurs.

## Test assessment

Direct coverage was added for `SkySettingsStore` and
`SkySettingsSnapshotCodecs`, including round trips, malformed values, partial
state, and split/merge behavior. Those tests cover the raw persistence layer
but not the controller load/save path where required profile and update fields
are currently lost.

Tests run:

- `cmake --build build-ralph --target skygate-ui-settings-store-tests
  skygate-ui-sky-settings-codecs-tests -j2`: PASS
- `ctest --test-dir build-ralph --output-on-failure -R
  'skygate-ui-(settings-store|sky-settings-codecs)-tests'`: PASS, 2/2

## Regression risk

Medium

The codec/storage changes are localized, but settings are loaded at app
startup and exposed through a public controller method. Dropping fields during
normal saves can silently reset user preferences once those controls are wired.

## Out-of-scope observations

- The atmosphere setting codecs accept finite but out-of-range numeric values,
  such as negative pressure or humidity above 1.0. The high-precision
  refraction calculator later treats those values as invalid and degrades the
  correction, but a follow-up could clamp or fall back earlier at the settings
  boundary.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
