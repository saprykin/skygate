## Task
- ID: HP-041C
- Title: Apply selected engine and request to search focus and tracking

## Status
READY

## Acceptance criteria claimed
- [x] Search focus uses the selected ephemeris engine request context
- [x] Tracked-target activation uses the selected ephemeris engine request
      context
- [x] Tracked-target recentering uses the selected ephemeris engine request
      context
- [x] Catalog/label-only search behavior remains unchanged
- [x] Existing tests pass

## Files changed
- `apps/skygate-ui/src/app/SkyContextController.hpp`
- `apps/skygate-ui/src/app/SkyContextController.cpp`
- `apps/skygate-ui/src/app/SkyContextControllerSearch.cpp`
- `apps/skygate-ui/tests/app/SkyContextControllerSearchTrackingTests.cpp`

## Important notes
- Added a controller initialization option that lets focused tests retain a
  provided request-sensitive fake engine instead of rebuilding through the
  factory on startup.
- Verification: `cmake --build build-ralph -j2` passed.
- Verification: `ctest --test-dir build-ralph --output-on-failure` passed
  124/124 tests; CALCEPH-dependent tests 33 and 34 were skipped by the
  existing build configuration.
