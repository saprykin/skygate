## Task
- ID: HP-025
- Title: Implement geometric solar-system vector calculation

## Status
READY

## Acceptance criteria claimed
- [x] Added `SolarSystemStateCalculator` under the high-precision engine boundary.
- [x] CALCEPH kernel provider can return geocentric geometric position vectors in AU.
- [x] Supported major solar-system body IDs map to NAIF target IDs.
- [x] Geometric vectors are converted to request-path RA/Dec without apparent corrections.
- [x] Unsupported body, missing kernel provider, out-of-range kernel status, and non-TDB epoch cases return structured status/warnings.
- [x] Added deterministic coverage including a compact JPL Horizons geometric vector smoke fixture.
- [x] Relevant ephemeris tests pass.

## Files changed
- `libs/skygate-ephemeris/CMakeLists.txt`
- `libs/skygate-ephemeris/src/engine/highprecision/HighPrecisionEphemerisEngine.hpp`
- `libs/skygate-ephemeris/src/engine/highprecision/CalcephKernelProvider.hpp`
- `libs/skygate-ephemeris/src/engine/highprecision/CalcephKernelProvider.cpp`
- `libs/skygate-ephemeris/src/engine/highprecision/SolarSystemStateCalculator.hpp`
- `libs/skygate-ephemeris/src/engine/highprecision/SolarSystemStateCalculator.cpp`
- `libs/skygate-ephemeris/tests/CMakeLists.txt`
- `libs/skygate-ephemeris/tests/fixtures/ephemeris/geometric_solar_system_smoke.csv`
- `libs/skygate-ephemeris/tests/highprecision/SolarSystemStateCalculatorTests.cpp`

## Important notes
- Full `ctest --test-dir build-ralph --output-on-failure` still fails only in unrelated `skygate-ui-qml-main-window-tests`, `QmlMainWindowTests::footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()` at `apps/skygate-ui/tests/qml/QmlMainWindowTests.cpp(219)`. This is tracked separately as `HP-056`.

## Verification
- `clang-format -i` on touched C++ headers/sources.
- `cmake -S . -B build-ralph`
- `cmake --build build-ralph --target skygate-ephemeris-solar-system-state-calculator-tests`
- `ctest --test-dir build-ralph --output-on-failure -R skygate-ephemeris-solar-system-state-calculator-tests`
- `ctest --test-dir build-ralph --output-on-failure -R 'skygate-ephemeris-(highprecision-engine|calceph-kernel-provider|solar-system-state-calculator|engine-baseline|engine-fallback|regression)-tests'`
- `cmake --build build-ralph`
- `ctest --test-dir build-ralph --output-on-failure` (115/116 passed; unrelated `HP-056` failure remains)

## Review fixes
- Review verdict addressed: NEEDS_FIX
- Findings addressed:
  - Finding title: Geometric requests can still be modified by apparent-place processing
    - Action: Fixed
    - Notes: The high-precision facade now bypasses the apparent-place collaborator for `Geometric`/`NoCorrections` solar-system requests and preserves geometric coordinates and applied corrections. Added facade regression coverage with a mutating apparent-place test double.
  - Finding title: Result metadata can dangle after provider lifetime ends
    - Action: Fixed
    - Notes: `EphemerisResultMetadata` now owns provenance and effective validity range values, and the CALCEPH provider copies kernel metadata into results. Added provider coverage proving returned metadata remains valid after provider destruction.
  - Finding title: Kernel provider accepts non-TDB epochs directly
    - Action: Fixed
    - Notes: `CalcephKernelProvider::computeGeometricState` now rejects non-TDB epochs with `Failed` plus `TimeScaleDataUnavailable` before calling the kernel handle. Added provider coverage for the boundary.
- Files changed during fix pass:
  - `libs/skygate-ephemeris/include/skygate/ephemeris/Types.hpp`
  - `libs/skygate-ephemeris/src/engine/highprecision/CalcephKernelProvider.cpp`
  - `libs/skygate-ephemeris/src/engine/highprecision/HighPrecisionEphemerisEngine.cpp`
  - `libs/skygate-ephemeris/tests/engine/EphemerisApiModelTests.cpp`
  - `libs/skygate-ephemeris/tests/engine/EphemerisEngineFallbackTests.cpp`
  - `libs/skygate-ephemeris/tests/highprecision/CalcephKernelProviderTests.cpp`
  - `libs/skygate-ephemeris/tests/highprecision/HighPrecisionEphemerisEngineTests.cpp`
  - `libs/skygate-ephemeris/tests/highprecision/SolarSystemStateCalculatorTests.cpp`
  - `.ralph/high-precision-ephemeris-engine/HP-025/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-025/fix.md`
- Tests run after fix:
  - `clang-format -i libs/skygate-ephemeris/include/skygate/ephemeris/Types.hpp libs/skygate-ephemeris/src/engine/highprecision/HighPrecisionEphemerisEngine.cpp libs/skygate-ephemeris/src/engine/highprecision/CalcephKernelProvider.cpp libs/skygate-ephemeris/tests/highprecision/HighPrecisionEphemerisEngineTests.cpp libs/skygate-ephemeris/tests/highprecision/CalcephKernelProviderTests.cpp libs/skygate-ephemeris/tests/engine/EphemerisApiModelTests.cpp libs/skygate-ephemeris/tests/engine/EphemerisEngineFallbackTests.cpp libs/skygate-ephemeris/tests/highprecision/SolarSystemStateCalculatorTests.cpp`
  - `cmake --build build-ralph --target skygate-ephemeris-highprecision-engine-tests skygate-ephemeris-calceph-kernel-provider-tests skygate-ephemeris-solar-system-state-calculator-tests skygate-ephemeris-api-model-tests skygate-ephemeris-engine-fallback-tests`
  - `ctest --test-dir build-ralph --output-on-failure -R 'skygate-ephemeris-(highprecision-engine|calceph-kernel-provider|solar-system-state-calculator|api-model|engine-fallback|engine-baseline|regression)-tests'`
  - `ctest --test-dir build-ralph --output-on-failure` (115/116 passed; unrelated `HP-056` failure remains)
- Remaining concerns: Full-suite CTest still fails in the unrelated tracked `skygate-ui-qml-main-window-tests` footer popup toolbar test.
