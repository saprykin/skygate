## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-044
- Title: Add Preferences ephemeris data controls
- Source: `IMPLEMENTATION_PLAN.md` HP-044
- Base ref: 7bf9ce8
- Head ref: 4be1e55

## Summary

The implementation adds an `Ephemeris Data` group, exposes status labels through
the controller/data manager, adds online-update, update, and clear-cache
controls, and adds QML coverage for fallback/installed display states.

The passive status display is mostly in place, and the test suite passes.
However, the Preferences controls do not provide a way to request/install the
optional DE441 long-range profile from the UI, so a required HP-044 user flow is
not actually reachable from Preferences.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: DE441 update path is not reachable from Preferences

Severity: MAJOR
File: `apps/skygate-ui/qml/preferences/PreferencesCatalogSection.qml`
Lines/functions: lines 226-353, `SkyContextController::updateEphemerisData`

Problem:
HP-044 requires the Preferences data controls to support ephemeris data updates
and to expose long-range DE441 installed/absent state. The implementation shows
the DE441 status, but the only update control is a generic `Update Data` button
that calls `SkyContextController::updateEphemerisData()`. That method always
activates `m_ephemerisUserSettings.preferredDataProfileId`, and the Preferences
UI added here provides no control to choose or request the DE441 long-range
profile. With the default `preferredDataProfileId` of `modern`, a user can see
`DE441 long range: Not installed` but cannot install it from this Preferences
surface.

Why it matters:
The spec explicitly says DE441 must be available through Preferences for users
who need long-range coverage, and HP-044 is the Preferences data-controls task.
Showing a passive DE441 status without an actionable DE441 install/update path
leaves that acceptance criterion incomplete.

Recommended fix:
Add a Preferences control that selects the desired ephemeris data profile or add
separate update actions for modern and DE441 data. Wire that selection through
the controller to `SkyEphemerisDataManager` so the DE441 profile can be staged,
verified, and activated from Preferences. Add QML/controller tests that start
from DE441 absent, invoke the DE441 update path, and verify the long-range
status changes to installed.

## Test assessment

Added QML tests cover fallback display, installed DE441 display, update enabled
state, clear-cache invocation, and QML warnings. They do not cover an actual
Preferences-triggered update activation, nor do they prove the optional DE441
profile can be selected or installed from the new controls.

Tests run:

- `cmake --build build-ralph --target skygate-ui-qml-preferences-catalog-tests -j2`
- `QT_QPA_PLATFORM=offscreen build-ralph/apps/skygate-ui/tests/skygate-ui-qml-preferences-catalog-tests -platform offscreen`
- `QT_QPA_PLATFORM=offscreen ctest --test-dir build-ralph -R 'skygate-ui-(qml-preferences-catalog-tests|qml-preferences-window-tests|sky-ephemeris-data-manager-tests|sky-context-controller-ephemeris-settings-tests|sky-settings-store-tests|sky-settings-codecs-tests)' --output-on-failure`
- `QT_QPA_PLATFORM=offscreen ctest --test-dir build-ralph --output-on-failure`

All 125 configured tests passed. The CALCEPH-dependent tests
`skygate-ephemeris-calceph-kernel-provider-tests` and
`skygate-ephemeris-solar-system-state-calculator-tests` were skipped by the
current build configuration.

## Regression risk

Medium

The new status labels and clear-cache path are localized, but the incomplete
update/profile selection flow affects a user-visible high-precision data
management requirement.

## Out-of-scope observations

No out-of-scope observations.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
