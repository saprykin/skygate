## Task fixed
  - ID: HP-025
  - Title: Implement geometric solar-system vector calculation
  - Source: IMPLEMENTATION_PLAN.md / specs/high-precision-ephemeris-engine.md

## Review input
  - Review verdict: NEEDS_FIX
  - Review report: `.ralph/high-precision-ephemeris-engine/HP-025/review.md`
  - Implementation handoff: `.ralph/high-precision-ephemeris-engine/HP-025/implementation.md`

## Summary
  Fixed the HP-025 review findings by preserving geometric solar-system results in the facade, making result metadata own copied provenance/range values, and enforcing TDB-only epochs at the CALCEPH provider boundary.

## Findings addressed
  - Finding title: Geometric requests can still be modified by apparent-place processing
  - Severity: MAJOR
  - Action: Fixed
  - File(s): `libs/skygate-ephemeris/src/engine/highprecision/HighPrecisionEphemerisEngine.cpp`, `libs/skygate-ephemeris/tests/highprecision/HighPrecisionEphemerisEngineTests.cpp`
  - What changed: Added a correction-flag gate so solar-system `Geometric`/`NoCorrections` requests bypass apparent-place processing. Added a facade test with a mutating apparent-place calculator to verify geometric RA/Dec and `appliedCorrections` are preserved.
  - Why this resolves the finding: A geometric request can no longer be altered by an apparent-place collaborator.

  - Finding title: Result metadata can dangle after provider lifetime ends
  - Severity: MAJOR
  - Action: Fixed
  - File(s): `libs/skygate-ephemeris/include/skygate/ephemeris/Types.hpp`, `libs/skygate-ephemeris/src/engine/highprecision/CalcephKernelProvider.cpp`, `libs/skygate-ephemeris/tests/engine/EphemerisApiModelTests.cpp`, `libs/skygate-ephemeris/tests/engine/EphemerisEngineFallbackTests.cpp`, `libs/skygate-ephemeris/tests/highprecision/CalcephKernelProviderTests.cpp`, `libs/skygate-ephemeris/tests/highprecision/SolarSystemStateCalculatorTests.cpp`
  - What changed: Changed `EphemerisResultMetadata` to own `dataSourceProvenance` and `effectiveDataValidityRange`, then copied CALCEPH kernel metadata into result metadata. Updated affected tests and added coverage that reads returned metadata after the provider is destroyed.
  - Why this resolves the finding: Public `CelestialBodyState` metadata no longer borrows provider-owned strings or date-range storage.

  - Finding title: Kernel provider accepts non-TDB epochs directly
  - Severity: MINOR
  - Action: Fixed
  - File(s): `libs/skygate-ephemeris/src/engine/highprecision/CalcephKernelProvider.cpp`, `libs/skygate-ephemeris/tests/highprecision/CalcephKernelProviderTests.cpp`
  - What changed: Added a `TimeScale::Tdb` guard in `CalcephKernelProvider::computeGeometricState` returning `Failed` with `TimeScaleDataUnavailable` before kernel evaluation. Added a provider test that verifies non-TDB input does not call the kernel handle.
  - Why this resolves the finding: The provider boundary now enforces the same TDB contract as the solar-system calculator and returns structured failure metadata.

## Tests run
  - `clang-format -i libs/skygate-ephemeris/include/skygate/ephemeris/Types.hpp libs/skygate-ephemeris/src/engine/highprecision/HighPrecisionEphemerisEngine.cpp libs/skygate-ephemeris/src/engine/highprecision/CalcephKernelProvider.cpp libs/skygate-ephemeris/tests/highprecision/HighPrecisionEphemerisEngineTests.cpp libs/skygate-ephemeris/tests/highprecision/CalcephKernelProviderTests.cpp libs/skygate-ephemeris/tests/engine/EphemerisApiModelTests.cpp libs/skygate-ephemeris/tests/engine/EphemerisEngineFallbackTests.cpp libs/skygate-ephemeris/tests/highprecision/SolarSystemStateCalculatorTests.cpp`: PASS
  - `cmake --build build-ralph --target skygate-ephemeris-highprecision-engine-tests skygate-ephemeris-calceph-kernel-provider-tests skygate-ephemeris-solar-system-state-calculator-tests skygate-ephemeris-api-model-tests skygate-ephemeris-engine-fallback-tests`: PASS
  - `ctest --test-dir build-ralph --output-on-failure -R 'skygate-ephemeris-(highprecision-engine|calceph-kernel-provider|solar-system-state-calculator|api-model|engine-fallback|engine-baseline|regression)-tests'`: PASS
  - `ctest --test-dir build-ralph --output-on-failure`: FAIL, 115/116 passed; unrelated tracked `skygate-ui-qml-main-window-tests` failure remains at `apps/skygate-ui/tests/qml/QmlMainWindowTests.cpp(219)`.

## Files changed
  - `.ralph/high-precision-ephemeris-engine/HP-025/fix.md`
  - `.ralph/high-precision-ephemeris-engine/HP-025/implementation.md`
  - `libs/skygate-ephemeris/include/skygate/ephemeris/Types.hpp`
  - `libs/skygate-ephemeris/src/engine/highprecision/CalcephKernelProvider.cpp`
  - `libs/skygate-ephemeris/src/engine/highprecision/HighPrecisionEphemerisEngine.cpp`
  - `libs/skygate-ephemeris/tests/engine/EphemerisApiModelTests.cpp`
  - `libs/skygate-ephemeris/tests/engine/EphemerisEngineFallbackTests.cpp`
  - `libs/skygate-ephemeris/tests/highprecision/CalcephKernelProviderTests.cpp`
  - `libs/skygate-ephemeris/tests/highprecision/HighPrecisionEphemerisEngineTests.cpp`
  - `libs/skygate-ephemeris/tests/highprecision/SolarSystemStateCalculatorTests.cpp`

## Remaining concerns
  Full-suite CTest still fails in unrelated tracked `skygate-ui-qml-main-window-tests`, `QmlMainWindowTests::footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()`.

## Final fixer status
  READY_FOR_REVIEW
