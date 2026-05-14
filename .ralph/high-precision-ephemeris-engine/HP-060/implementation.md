## Task
- ID: HP-060
- Title: Wire production ephemeris data acquisition

## Status
READY

## Acceptance criteria claimed
- [x] Production startup loads a packaged ephemeris manifest resource
- [x] Startup passes manifest, bundled root, and writable cache root into the
  controller
- [x] Profile update stages each manifest asset before verification and atomic
  activation
- [x] Source URL acquisition supports local file URLs and HTTP(S) URLs
- [x] Missing manifest, profile, source, download, and activation failures
  produce non-empty status text
- [x] `Install DE441` keeps targeting the explicit `de441-long-range` profile
- [x] Existing staged-resource activation remains available for tests
- [x] Existing tests pass

## Files changed
- `apps/skygate-ui/CMakeLists.txt`
- `apps/skygate-ui/resources/ephemeris.qrc`
- `apps/skygate-ui/resources/ephemeris/manifest.json`
- `apps/skygate-ui/src/app/SkyContextController.cpp`
- `apps/skygate-ui/src/app/SkyContextController.hpp`
- `apps/skygate-ui/src/ephemeris/SkyEphemerisDataManager.cpp`
- `apps/skygate-ui/src/ephemeris/SkyEphemerisDataManager.hpp`
- `apps/skygate-ui/src/main.cpp`
- `apps/skygate-ui/tests/ephemeris/SkyEphemerisDataManagerTests.cpp`

## Important notes
- `ctest --test-dir build-ralph --output-on-failure` passed: 127/127
  executed tests passed, with
  `skygate-ephemeris-solar-system-state-calculator-tests` skipped by
  configuration.

## Review fixes
- Review verdict addressed: NEEDS_FIX
- Findings addressed:
  - Packaged startup does not load the manifest
    - Action: Fixed
    - Notes: Moved the ephemeris qrc into the executable and made the
      packaged smoke test require the manifest-loaded startup log.
  - Production manifest checksums cannot verify
    - Action: Fixed
    - Notes: Replaced placeholder checksums with SHA-256 values and sizes for
      the exact listed source assets.
  - DE441 profile references assets from another profile
    - Action: Fixed
    - Notes: Added long-range-owned support assets so every profile asset has
      a matching `profileId`.
  - Bundled modern data is declared but not packaged
    - Action: Fixed
    - Notes: Marked the production modern profile unbundled until release
      packaging supplies the actual modern data files, and prevented unbundled
      profiles from being exposed as bundled fallback data.
- Files changed during fix pass:
  - `apps/skygate-ui/CMakeLists.txt`
  - `apps/skygate-ui/resources/ephemeris/manifest.json`
  - `apps/skygate-ui/src/ephemeris/SkyEphemerisDataManager.cpp`
  - `apps/skygate-ui/tests/CMakeLists.txt`
  - `apps/skygate-ui/tests/PackagedAppSmoke.cmake`
  - `apps/skygate-ui/tests/ephemeris/SkyEphemerisDataManagerTests.cpp`
  - `.ralph/high-precision-ephemeris-engine/HP-060/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-060/fix.md`
- Tests run after fix:
  - `cmake --build build-ralph --target
    skygate-ui-sky-ephemeris-data-manager-tests skygate-ui`: PASS
  - `ctest --test-dir build-ralph --output-on-failure -R
    'skygate-ui-sky-ephemeris-data-manager-tests|
    skygate-ui-packaged-app-smoke'`:
    PASS
  - `ctest --test-dir build-ralph --output-on-failure -R
    'skygate-ui-acceptance-matrix-tests|
    skygate-ui-context-controller-ephemeris-settings-tests'`:
    PASS
  - `ctest --test-dir build-ralph --output-on-failure`: PASS
    (126 passed, 1 skipped)
- Remaining concerns: None.
