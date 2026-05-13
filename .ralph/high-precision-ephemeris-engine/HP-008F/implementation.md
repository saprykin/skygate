## Task
- ID: HP-008F
- Title: Wire real high-precision construction once dependencies exist

## Status
READY

## Acceptance criteria claimed
- [x] Factory can construct `HighPrecisionEphemerisEngine` from a high-precision request when data, manifest, time-scale service, Earth-orientation provider, and kernel runtime are available
- [x] Active data snapshot, manifest data-set metadata, time-scale service, Earth-orientation provider, and CALCEPH kernel provider are propagated into the created engine
- [x] Strict high-precision failures return structured diagnostics with no engine
- [x] Explicit fallback requests return a simple engine with warning diagnostics when high-precision construction cannot proceed
- [x] Factory tests cover successful fake-runtime high-precision construction, metadata/provider propagation, strict failure, and fallback behavior
- [x] Existing tests pass

## Files changed
- `libs/skygate-ephemeris/include/skygate/ephemeris/EphemerisEngineFactory.hpp`
- `libs/skygate-ephemeris/src/engine/simple/SimpleEphemerisEngine.cpp`
- `libs/skygate-ephemeris/src/engine/highprecision/HighPrecisionEphemerisEngine.hpp`
- `libs/skygate-ephemeris/tests/engine/EphemerisApiModelTests.cpp`
- `libs/skygate-ephemeris/tests/engine/EphemerisEngineFactoryBehaviorTests.cpp`
- `libs/skygate-ephemeris/tests/engine/EphemerisEngineFactorySelectionTests.cpp`
- `libs/skygate-ephemeris/tests/highprecision/HighPrecisionEphemerisEngineTests.cpp`
- `.ralph/high-precision-ephemeris-engine/HP-008F/implementation.md`

## Important notes
- `ctest --test-dir build-ralph --output-on-failure` passed: 58/58 tests.
