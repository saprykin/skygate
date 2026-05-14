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
