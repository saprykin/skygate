## Task fixed
  - ID: HP-039
  - Title: Add ephemeris user settings snapshot
  - Source: Spec: Engine Selection; Settings

## Review input
  - Review verdict: NEEDS_FIX
  - Review report: `.ralph/high-precision-ephemeris-engine/HP-039/review.md`
  - Implementation handoff:
    `.ralph/high-precision-ephemeris-engine/HP-039/implementation.md`

## Summary
  Fixed the controller settings path so it preserves the full ephemeris user
  settings snapshot, including correction preset, preferred data profile, and
  update preferences. Settings reloads now notify scene consumers after
  rebuilding the ephemeris engine.

## Findings addressed
  - Finding title: Controller Drops Profile And Update Settings
  - Severity: MAJOR
  - Action: Fixed
  - File(s):
    `apps/skygate-ui/src/app/SkyContextController.hpp`,
    `apps/skygate-ui/src/app/SkyContextController.cpp`,
    `apps/skygate-ui/src/app/SkyContextControllerSettings.cpp`,
    `apps/skygate-ui/tests/app/SkyContextControllerEphemerisSettingsTests.cpp`,
    `apps/skygate-ui/tests/CMakeLists.txt`
  - What changed: Added controller-owned ephemeris user settings state, loaded
    the whole snapshot, and saved it back before overlaying live engine option
    fields.
  - Why this resolves the finding: Controller save no longer overwrites
    profile or update settings with defaults after loading persisted settings.

  - Finding title: Ephemeris Reload Does Not Notify Scene Consumers
  - Severity: MAJOR
  - Action: Fixed
  - File(s):
    `apps/skygate-ui/src/app/SkyContextControllerSettings.cpp`,
    `apps/skygate-ui/tests/app/SkyContextControllerEphemerisSettingsTests.cpp`
  - What changed: `loadSettings()` emits `skyContextChanged()` after applying
    persisted ephemeris settings and rebuilding the engine.
  - Why this resolves the finding: Runtime settings reload now tells scene
    consumers to recompute after engine or correction-option changes.

## Tests run
  - `cmake -S . -B build-ralph`: PASS
  - `cmake --build build-ralph --target`
    `skygate-ui-context-controller-ephemeris-settings-tests`
    `skygate-ui-settings-store-tests`
    `skygate-ui-sky-settings-codecs-tests -j2`: PASS
  - `ctest --test-dir build-ralph --output-on-failure -R`
    `'ephemeris-settings|settings-store|sky-settings-codecs'`: PASS
  - `ctest --test-dir build-ralph --output-on-failure`: PASS, 124/124 tests
    passed. The two configured CALCEPH-dependent tests were skipped.

## Files changed
  - `.ralph/high-precision-ephemeris-engine/HP-039/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-039/fix.md`
  - `apps/skygate-ui/src/app/SkyContextController.hpp`
  - `apps/skygate-ui/src/app/SkyContextController.cpp`
  - `apps/skygate-ui/src/app/SkyContextControllerSettings.cpp`
  - `apps/skygate-ui/tests/app/SkyContextControllerEphemerisSettingsTests.cpp`
  - `apps/skygate-ui/tests/CMakeLists.txt`

## Remaining concerns
  None.

## Final fixer status
  READY_FOR_REVIEW
