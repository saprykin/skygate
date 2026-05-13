## Task
- ID: HP-023
- Title: Add CALCEPH kernel loading and selection provider

## Status
READY

## Acceptance criteria claimed
- [x] `CalcephKernelProvider` added under the high-precision engine boundary
- [x] Modern and optional long-range manifest profile selection implemented
- [x] Active data snapshot kernel path metadata exposed to the provider
- [x] Kernel file existence, size, checksum, open/close, and date-range status handling added
- [x] Focused provider tests added for selection and error cases
- [x] Full build in `build-ralph` succeeds

## Files changed
- `libs/skygate-ephemeris/src/engine/highprecision/CalcephKernelProvider.hpp`
- `libs/skygate-ephemeris/src/engine/highprecision/CalcephKernelProvider.cpp`
- `libs/skygate-ephemeris/include/skygate/ephemeris/EphemerisDataSnapshot.hpp`
- `apps/skygate-ui/src/ephemeris/SkyEphemerisDataManager.cpp`
- `libs/skygate-ephemeris/CMakeLists.txt`
- `libs/skygate-ephemeris/tests/CMakeLists.txt`
- `libs/skygate-ephemeris/tests/highprecision/CalcephKernelProviderTests.cpp`

## Important notes
- `cmake --build build-ralph` succeeds.
- `ctest --test-dir build-ralph --output-on-failure -R skygate-ephemeris-calceph-kernel-provider-tests` passes.
- Full `ctest --test-dir build-ralph --output-on-failure` ran 114/115 passing; the only failure was the unrelated recurring `skygate-ui-qml-main-window-tests` footer popup toolbar toggle failure. It is tracked as `HP-055` in `IMPLEMENTATION_PLAN.md`.
