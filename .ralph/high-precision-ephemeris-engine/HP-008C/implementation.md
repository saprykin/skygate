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
