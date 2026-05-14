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

## Review fixes

- Review verdict addressed: NEEDS_FIX
- Findings addressed:
  - Finding title: Request overload mixes request epoch and context time
    - Action: Fixed
    - Notes: Lunar phase and illumination now use `EphemerisRequest::epoch`
      instead of `request.context.utcTime`.
  - Finding title: Integration test does not prove selected request options
    - Action: Fixed
    - Notes: The controller integration test now makes night-condition
      altitude/event samples depend on selected request engine kind and
      correction flags, and asserts the fake engine observed them.
- Files changed during fix pass:
  - `libs/skygate-ephemeris/src/NightConditionsCalculator.cpp`
  - `libs/skygate-ephemeris/tests/events/NightConditionsCalculatorTests.cpp`
  - `apps/skygate-ui/tests/app/SkyContextControllerNightCatalogTests.cpp`
  - `.ralph/high-precision-ephemeris-engine/HP-041F/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-041F/fix.md`
- Tests run after fix:
  - `clang-format -i libs/skygate-ephemeris/src/NightConditionsCalculator.cpp
    libs/skygate-ephemeris/tests/events/NightConditionsCalculatorTests.cpp
    apps/skygate-ui/tests/app/SkyContextControllerNightCatalogTests.cpp`: PASS
  - `cmake --build build-ralph --target
    skygate-ephemeris-night-conditions-calculator-tests
    skygate-ui-context-controller-night-catalog-tests`: PASS
  - `ctest --test-dir build-ralph --output-on-failure -R
    'skygate-(ephemeris-night-conditions-calculator-tests|
    ui-context-controller-night-catalog-tests)'`: PASS
  - `ctest --test-dir build-ralph --output-on-failure`: PASS
- Remaining concerns: None.
