## Task
- ID: HP-021
- Title: Add `SkyEphemerisDataManager`

## Status
READY

## Acceptance criteria claimed
- [x] Application-layer `SkyEphemerisDataManager` added and owned by `SkyContextController`
- [x] Manager reports bundled, installed, and missing-installed fallback status
- [x] Manager provides active immutable ephemeris data snapshots for engine factory wiring
- [x] Offline bundled fallback remains active when no installed data is available
- [x] Active data revision signals emit when active ephemeris data changes
- [x] Unit tests added for bundled status, installed status, missing installed fallback, revision emission, controller ownership, and catalog independence

## Files changed
- `apps/skygate-ui/CMakeLists.txt`
- `apps/skygate-ui/src/app/SkyContextController.cpp`
- `apps/skygate-ui/src/app/SkyContextController.hpp`
- `apps/skygate-ui/src/app/SkyContextControllerSettings.cpp`
- `apps/skygate-ui/src/ephemeris/SkyEphemerisDataManager.cpp`
- `apps/skygate-ui/src/ephemeris/SkyEphemerisDataManager.hpp`
- `apps/skygate-ui/tests/CMakeLists.txt`
- `apps/skygate-ui/tests/ephemeris/SkyEphemerisDataManagerTests.cpp`

## Important notes
- `cmake --build build-ralph -j2` completed successfully.
- `ctest --test-dir build-ralph -R skygate-ui-sky-ephemeris-data-manager-tests --output-on-failure` passed.
- Full `ctest --test-dir build-ralph --output-on-failure` reported 113/114 passing. The only failure was the pre-existing unrelated `skygate-ui-qml-main-window-tests` footer popup toolbar assertion already tracked by HP-054.
