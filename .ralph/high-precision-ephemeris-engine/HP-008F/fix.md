## Task fixed
  - ID: HP-008F
  - Title: Wire real high-precision construction once dependencies exist
  - Source: `IMPLEMENTATION_PLAN.md`

## Review input
  - Review verdict: NEEDS_FIX
  - Review report: `.ralph/high-precision-ephemeris-engine/HP-008F/review.md`
  - Implementation handoff: `.ralph/high-precision-ephemeris-engine/HP-008F/implementation.md`

## Summary
  Fixed the two major review findings. Factory diagnostics are now delivered through the request diagnostics sink, and factory-created high-precision engines now receive a concrete apparent/topocentric calculator wired to the time-scale service, Earth-orientation provider, and frame transformer.

## Findings addressed
  - Finding title: Diagnostics sink is accepted but never used
  - Severity: MAJOR
  - Action: Fixed
  - File(s): `libs/skygate-ephemeris/include/skygate/ephemeris/EphemerisEngineFactory.hpp`, `libs/skygate-ephemeris/src/engine/simple/SimpleEphemerisEngine.cpp`, `libs/skygate-ephemeris/tests/engine/EphemerisEngineFactoryBehaviorTests.cpp`
  - What changed: Added `IEphemerisDiagnosticsSink::recordFactoryCreationDiagnostic(...)` and publish factory result diagnostics to the request sink before returning. Added recording-sink coverage for fallback warnings and strict failure errors.
  - Why this resolves the finding: Callers that provide `request.diagnosticsSink` now observe the same creation diagnostics returned in `EphemerisEngineFactoryResult::diagnostics`.

  - Finding title: Apparent/topocentric correction path is not wired for factory-created engines
  - Severity: MAJOR
  - Action: Fixed
  - File(s): `libs/skygate-ephemeris/CMakeLists.txt`, `libs/skygate-ephemeris/src/engine/highprecision/ApparentPlaceCalculator.hpp`, `libs/skygate-ephemeris/src/engine/highprecision/ApparentPlaceCalculator.cpp`, `libs/skygate-ephemeris/src/engine/simple/SimpleEphemerisEngine.cpp`, `libs/skygate-ephemeris/tests/engine/EphemerisEngineFactoryBehaviorTests.cpp`
  - What changed: Added a production apparent-place calculator and wired it into factory-created high-precision dependencies. The calculator uses the request time-scale service, Earth-orientation provider, and frame transformer for apparent/topocentric requests. Added factory behavior coverage that verifies the created engine consumes the propagated time/EOP providers.
  - Why this resolves the finding: Factory-created high-precision engines no longer fall back to the no-op apparent calculator for non-geometric requests, so the provider dependencies accepted by the factory are part of the correction path.

## Tests run
  - `cmake --build build-ralph --target skygate-ephemeris skygate-ephemeris-api-model-tests skygate-ephemeris-engine-interface-migration-tests skygate-ephemeris-engine-factory-selection-tests skygate-ephemeris-engine-factory-behavior-tests skygate-ephemeris-highprecision-engine-tests skygate-ephemeris-calceph-kernel-provider-tests skygate-ephemeris-solar-system-state-calculator-tests skygate-ephemeris-data-manifest-tests skygate-ephemeris-data-activation-tests skygate-ephemeris-delta-t-provider-tests skygate-ephemeris-earth-orientation-provider-tests skygate-ephemeris-leap-second-provider-tests skygate-ephemeris-time-scale-service-tests skygate-ephemeris-public-header-macro-compat-tests skygate-ephemeris-engine-baseline-tests skygate-ephemeris-engine-fallback-tests skygate-ephemeris-regression-tests` - PASS
  - `ctest --test-dir build-ralph --output-on-failure -L highprecision` - PASS
  - `ctest --test-dir build-ralph --output-on-failure -R '^(skygate-ephemeris-(api-model|engine-interface-migration|engine-factory-selection|engine-factory-behavior|public-header-macro-compat|engine-baseline|engine-fallback|regression)-tests)$'` - PASS
  - `ctest --test-dir build-ralph --output-on-failure` - PASS, 58/58
  - Dependency-enabled high-precision build with `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=ON` - NOT RUN; current `build-ralph` is configured OFF and has no `calceph_DIR`.

## Files changed
  - `.ralph/high-precision-ephemeris-engine/HP-008F/fix.md`
  - `.ralph/high-precision-ephemeris-engine/HP-008F/implementation.md`
  - `libs/skygate-ephemeris/CMakeLists.txt`
  - `libs/skygate-ephemeris/include/skygate/ephemeris/EphemerisEngineFactory.hpp`
  - `libs/skygate-ephemeris/src/engine/highprecision/ApparentPlaceCalculator.hpp`
  - `libs/skygate-ephemeris/src/engine/highprecision/ApparentPlaceCalculator.cpp`
  - `libs/skygate-ephemeris/src/engine/simple/SimpleEphemerisEngine.cpp`
  - `libs/skygate-ephemeris/tests/engine/EphemerisEngineFactoryBehaviorTests.cpp`

## Remaining concerns
  Dependency-enabled high-precision configure/test remains unverified in this environment because `build-ralph` is high-precision disabled and CALCEPH is not configured.

## Final fixer status
  READY_FOR_REVIEW
