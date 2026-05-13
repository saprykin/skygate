## Task fixed
  - ID: HP-008D
  - Title: Implement factory selection for simple and unavailable high precision
  - Source: IMPLEMENTATION_PLAN.md; spec/high-precision-ephemeris-engine.md

## Review input
  - Review verdict: NEEDS_FIX
  - Review report: .ralph/high-precision-ephemeris-engine/HP-008D/review.md
  - Implementation handoff: .ralph/high-precision-ephemeris-engine/HP-008D/implementation.md

## Summary
  Fixed the factory request default so high-precision creation is strict unless callers explicitly opt into simple-engine fallback. Added regression coverage for the default high-precision request path.

## Findings addressed
  - Finding title: High-precision fallback is allowed by default
  - Severity: MAJOR
  - Action: Fixed
  - File(s): libs/skygate-ephemeris/include/skygate/ephemeris/EphemerisEngineFactory.hpp; libs/skygate-ephemeris/tests/engine/EphemerisApiModelTests.cpp; libs/skygate-ephemeris/tests/engine/EphemerisEngineFactorySelectionTests.cpp
  - What changed: `EphemerisEngineFactoryRequest::fallbackPolicy` now defaults to `StrictHighPrecision`; the API default test expects strict fallback policy; factory selection tests now verify that a default high-precision request fails with `FailedStrictHighPrecisionUnavailable` and no engine.
  - Why this resolves the finding: A caller requesting `HighPrecision` without explicitly setting `AllowSimpleEngineFallback` can no longer receive a silent simple-engine fallback.

## Tests run
  - `clang-format -i libs/skygate-ephemeris/include/skygate/ephemeris/EphemerisEngineFactory.hpp libs/skygate-ephemeris/tests/engine/EphemerisApiModelTests.cpp libs/skygate-ephemeris/tests/engine/EphemerisEngineFactorySelectionTests.cpp`: PASS
  - `cmake --build build-ralph --target skygate-ephemeris-engine-factory-selection-tests skygate-ephemeris-api-model-tests`: PASS
  - `ctest --test-dir build-ralph -R "skygate-ephemeris-(api-model|engine-factory-selection)-tests" --output-on-failure`: PASS
  - `ctest --test-dir build-ralph -R "skygate-ephemeris-(api-model|engine-interface-migration|engine-factory-selection)-tests" --output-on-failure`: PASS
  - `ctest --test-dir build-ralph --output-on-failure`: FAIL, 104/105 tests passed; the only failure was the pre-existing unrelated `skygate-ui-qml-main-window-tests` failure at `apps/skygate-ui/tests/qml/QmlMainWindowTests.cpp(219)`, already tracked by HP-052.

## Files changed
  - libs/skygate-ephemeris/include/skygate/ephemeris/EphemerisEngineFactory.hpp
  - libs/skygate-ephemeris/tests/engine/EphemerisApiModelTests.cpp
  - libs/skygate-ephemeris/tests/engine/EphemerisEngineFactorySelectionTests.cpp
  - .ralph/high-precision-ephemeris-engine/HP-008D/implementation.md
  - .ralph/high-precision-ephemeris-engine/HP-008D/fix.md

## Remaining concerns
  Full-suite CTest still fails on the unrelated HP-052 QML main-window regression.

## Final fixer status
  READY_FOR_REVIEW
