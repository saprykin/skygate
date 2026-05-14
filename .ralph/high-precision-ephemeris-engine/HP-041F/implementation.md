## Task
- ID: HP-041F
- Title: Apply selected engine and request to night conditions

## Status
READY

## Acceptance criteria claimed
- [x] Night-conditions icon uses the selected ephemeris request context
- [x] Night-conditions calculations use the selected ephemeris request context
- [x] Sun/Moon state and rise/set summary calculations switch through the
  request-based engine path
- [x] Integration coverage added for selected-engine request routing
- [x] Existing tests pass

## Files changed
- `libs/skygate-ephemeris/include/skygate/ephemeris/NightConditionsCalculator.hpp`
- `libs/skygate-ephemeris/src/NightConditionsCalculator.cpp`
- `apps/skygate-ui/src/app/SkyContextControllerNightConditions.cpp`
- `apps/skygate-ui/tests/app/SkyContextControllerNightCatalogTests.cpp`

## Important notes
- Full `ctest --test-dir build-ralph --output-on-failure` passed with 124
  tests run; the two CALCEPH-dependent tests were skipped by the existing build
  configuration.
