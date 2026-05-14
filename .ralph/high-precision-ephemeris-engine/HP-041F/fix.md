## Task fixed
  - ID: HP-041F
  - Title: Apply selected engine and request to night conditions
  - Source: IMPLEMENTATION_PLAN.md

## Review input
  - Review verdict: NEEDS_FIX
  - Review report: `.ralph/high-precision-ephemeris-engine/HP-041F/review.md`
  - Implementation handoff:
    `.ralph/high-precision-ephemeris-engine/HP-041F/implementation.md`

## Summary
  Fixed night-condition request handling so lunar phase and illumination use the
  explicit ephemeris request epoch. Strengthened integration coverage so icon
  and summary values depend on the controller-selected request engine kind and
  correction flags.

## Findings addressed
  - Finding title: Request overload mixes request epoch and context time
  - Severity: MAJOR
  - Action: Fixed
  - File(s):
    `libs/skygate-ephemeris/src/NightConditionsCalculator.cpp`,
    `libs/skygate-ephemeris/tests/events/NightConditionsCalculatorTests.cpp`
  - What changed: Lunar-cycle calculation now reads
    `EphemerisRequest::epoch`, and a regression test covers a request whose
    epoch intentionally differs from `request.context.utcTime`.
  - Why this resolves the finding: All night-condition values in the request
    overload now derive from the same explicit request epoch contract instead
    of mixing in the legacy context time for moon phase.

  - Finding title: Integration test does not prove selected request options
  - Severity: MAJOR
  - Action: Fixed
  - File(s):
    `apps/skygate-ui/tests/app/SkyContextControllerNightCatalogTests.cpp`
  - What changed: The fake night engine now changes Sun altitude and event
    samples only when it receives selected `HighPrecision` plus `LightTime`
    request options. The test asserts icon kind, timed summary rows, request
    calls, and observed request options.
  - Why this resolves the finding: A regression back to the calculator context
    adapter, or any path that drops controller-selected options, now produces
    non-selected fake outputs and fails the test.

## Tests run
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
    124 tests discovered; 122 passed and 2 CALCEPH-dependent tests skipped.

## Files changed
  - `libs/skygate-ephemeris/src/NightConditionsCalculator.cpp`
  - `libs/skygate-ephemeris/tests/events/NightConditionsCalculatorTests.cpp`
  - `apps/skygate-ui/tests/app/SkyContextControllerNightCatalogTests.cpp`
  - `.ralph/high-precision-ephemeris-engine/HP-041F/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-041F/fix.md`

## Remaining concerns
  None.

## Final fixer status
  READY_FOR_REVIEW
