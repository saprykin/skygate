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

## Review fixes
- Review verdict addressed: NEEDS_FIX
- Findings addressed:
  - Diagnostics sink is accepted but never used
    - Action: Fixed
    - Notes: Defined the public factory diagnostics sink contract and publish returned factory diagnostics to the request sink for strict failures, fallback warnings, and invalid requests.
  - Apparent/topocentric correction path is not wired for factory-created engines
    - Action: Fixed
    - Notes: Added a production apparent-place calculator, wired it into factory-created high-precision engines, and covered provider consumption through a factory behavior test. In the current high-precision-disabled `build-ralph` configuration, ERFA transforms remain unavailable by design, but the constructed engine now consumes the request time-scale and Earth-orientation providers on the apparent/topocentric path.
- Files changed during fix pass:
  - `libs/skygate-ephemeris/CMakeLists.txt`
  - `libs/skygate-ephemeris/include/skygate/ephemeris/EphemerisEngineFactory.hpp`
  - `libs/skygate-ephemeris/src/engine/highprecision/ApparentPlaceCalculator.hpp`
  - `libs/skygate-ephemeris/src/engine/highprecision/ApparentPlaceCalculator.cpp`
  - `libs/skygate-ephemeris/src/engine/simple/SimpleEphemerisEngine.cpp`
  - `libs/skygate-ephemeris/tests/engine/EphemerisEngineFactoryBehaviorTests.cpp`
  - `.ralph/high-precision-ephemeris-engine/HP-008F/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-008F/fix.md`
- Tests run after fix:
  - `cmake --build build-ralph --target skygate-ephemeris skygate-ephemeris-api-model-tests skygate-ephemeris-engine-interface-migration-tests skygate-ephemeris-engine-factory-selection-tests skygate-ephemeris-engine-factory-behavior-tests skygate-ephemeris-highprecision-engine-tests skygate-ephemeris-calceph-kernel-provider-tests skygate-ephemeris-solar-system-state-calculator-tests skygate-ephemeris-data-manifest-tests skygate-ephemeris-data-activation-tests skygate-ephemeris-delta-t-provider-tests skygate-ephemeris-earth-orientation-provider-tests skygate-ephemeris-leap-second-provider-tests skygate-ephemeris-time-scale-service-tests skygate-ephemeris-public-header-macro-compat-tests skygate-ephemeris-engine-baseline-tests skygate-ephemeris-engine-fallback-tests skygate-ephemeris-regression-tests` - PASS
  - `ctest --test-dir build-ralph --output-on-failure -L highprecision` - PASS
  - `ctest --test-dir build-ralph --output-on-failure -R '^(skygate-ephemeris-(api-model|engine-interface-migration|engine-factory-selection|engine-factory-behavior|public-header-macro-compat|engine-baseline|engine-fallback|regression)-tests)$'` - PASS
  - `ctest --test-dir build-ralph --output-on-failure` - PASS, 58/58
- Remaining concerns:
  - Dependency-enabled high-precision configure/test was not run because `build-ralph` is configured with `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=OFF` and `calceph_DIR` is not available in that build.
