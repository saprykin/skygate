## Task fixed
  - ID: HP-008C
  - Title: Preserve simple-engine compatibility overloads
  - Source: IMPLEMENTATION_PLAN.md

## Review input
  - Review verdict: NEEDS_FIX
  - Review report: .ralph/high-precision-ephemeris-engine/HP-008C/review.md
  - Implementation handoff: .ralph/high-precision-ephemeris-engine/HP-008C/implementation.md

## Summary
  Added a simple-engine initializer-list compatibility overload so existing
  callers using `createEphemerisEngine({})` continue to compile and create an
  empty simple engine after the factory request overload was introduced.

## Findings addressed
  - Finding title: Empty-brace factory calls are now ambiguous
  - Severity: MAJOR
  - Action: Fixed
  - File(s): `libs/skygate-ephemeris/include/skygate/ephemeris/EphemerisEngineFactory.hpp`, `libs/skygate-ephemeris/src/engine/simple/SimpleEphemerisEngine.cpp`, `libs/skygate-ephemeris/tests/engine/EphemerisApiModelTests.cpp`
  - What changed: Added `createEphemerisEngine(std::initializer_list<CelestialBody>)`, routed it through the existing span compatibility path, and added API coverage for `createEphemerisEngine({})`.
  - Why this resolves the finding: Empty-brace calls now select the initializer-list overload and no longer have to choose between the request and span overloads, preserving the simple-engine compatibility path.

## Tests run
  - `cmake --build build-ralph --target skygate-ephemeris-api-model-tests skygate-ephemeris-engine-baseline-tests skygate-ephemeris-engine-fallback-tests skygate-ephemeris-regression-tests -j2`: PASS
  - `ctest --test-dir build-ralph --output-on-failure -R 'skygate-ephemeris-(api-model|engine-baseline|engine-fallback|regression)-tests'`: PASS
  - `ctest --test-dir build-ralph --output-on-failure`: FAIL, 103/104 passed; the only failure is the pre-existing HP-052 `skygate-ui-qml-main-window-tests` failure at `apps/skygate-ui/tests/qml/QmlMainWindowTests.cpp(219)`.

## Files changed
  - `libs/skygate-ephemeris/include/skygate/ephemeris/EphemerisEngineFactory.hpp`
  - `libs/skygate-ephemeris/src/engine/simple/SimpleEphemerisEngine.cpp`
  - `libs/skygate-ephemeris/tests/engine/EphemerisApiModelTests.cpp`
  - `.ralph/high-precision-ephemeris-engine/HP-008C/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-008C/fix.md`

## Remaining concerns
  None.

## Final fixer status
  READY_FOR_REVIEW
