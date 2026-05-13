## Task
- ID: HP-008C
- Title: Preserve simple-engine compatibility overloads

## Status
READY

## Acceptance criteria claimed
- [x] Existing no-argument, catalog, and body-span `createEphemerisEngine(...)` overloads remain available.
- [x] Compatibility overloads route through the request/result factory implementation for simple-engine requests.
- [x] Current simple-engine behavior is preserved for callers that do not pass a factory request.
- [x] Factory/API tests cover the compatibility overloads and simple request/result creation.
- [x] Golden simple-engine baseline, fallback, and regression tests pass.

## Files changed
- `libs/skygate-ephemeris/include/skygate/ephemeris/EphemerisEngineFactory.hpp`
- `libs/skygate-ephemeris/src/engine/simple/SimpleEphemerisEngine.cpp`
- `libs/skygate-ephemeris/tests/engine/EphemerisApiModelTests.cpp`

## Important notes
- Focused verification passed:
  `ctest --test-dir build-ralph --output-on-failure -R 'skygate-ephemeris-(api-model|engine-baseline|engine-fallback|regression)-tests'`.
- Full-suite verification was run with `ctest --test-dir build-ralph --output-on-failure`.
  103/104 tests passed. The only failure was the pre-existing
  `skygate-ui-qml-main-window-tests` failure tracked by HP-052.

## Review fixes
- Review verdict addressed: NEEDS_FIX
- Findings addressed:
  - Finding title: Empty-brace factory calls are now ambiguous
  - Action: Fixed
  - Notes: Added an initializer-list compatibility overload so
    `createEphemerisEngine({})` resolves to the simple-engine compatibility path
    instead of being ambiguous between factory request and span overloads. Added
    API coverage that compiles and exercises the empty-brace call.
- Files changed during fix pass:
  - `libs/skygate-ephemeris/include/skygate/ephemeris/EphemerisEngineFactory.hpp`
  - `libs/skygate-ephemeris/src/engine/simple/SimpleEphemerisEngine.cpp`
  - `libs/skygate-ephemeris/tests/engine/EphemerisApiModelTests.cpp`
  - `.ralph/high-precision-ephemeris-engine/HP-008C/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-008C/fix.md`
- Tests run after fix:
  - `cmake --build build-ralph --target skygate-ephemeris-api-model-tests skygate-ephemeris-engine-baseline-tests skygate-ephemeris-engine-fallback-tests skygate-ephemeris-regression-tests -j2`: PASS
  - `ctest --test-dir build-ralph --output-on-failure -R 'skygate-ephemeris-(api-model|engine-baseline|engine-fallback|regression)-tests'`: PASS
  - `ctest --test-dir build-ralph --output-on-failure`: FAIL, 103/104 passed; only the pre-existing HP-052
    `skygate-ui-qml-main-window-tests` failure reproduced at
    `apps/skygate-ui/tests/qml/QmlMainWindowTests.cpp(219)`.
- Remaining concerns:
  - None for HP-008C. The unrelated HP-052 QML failure remains outside this
    task.
