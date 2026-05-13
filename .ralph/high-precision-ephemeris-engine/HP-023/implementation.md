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

## Review fixes
- Review verdict addressed: NEEDS_FIX
- Findings addressed:
  - Finding title: Active kernel snapshot can misidentify the selected manifest asset
  - Action: Fixed
  - Notes: Persisted active ephemeris data now carries the installed solar-system kernel asset ID and profile ID. The active data snapshot only returns a kernel asset when the requested asset ID matches the installed kernel, and `CalcephKernelProvider` rejects snapshots that expose a different kernel asset or profile before using the active file path.
- Files changed during fix pass:
  - `apps/skygate-ui/src/settings/SkySettingsStore.hpp`
  - `apps/skygate-ui/src/settings/SkySettingsStore.cpp`
  - `apps/skygate-ui/src/ephemeris/SkyEphemerisDataManager.cpp`
  - `apps/skygate-ui/tests/settings/SkySettingsStoreTests.cpp`
  - `apps/skygate-ui/tests/ephemeris/SkyEphemerisDataManagerTests.cpp`
  - `libs/skygate-ephemeris/include/skygate/ephemeris/EphemerisDataSnapshot.hpp`
  - `libs/skygate-ephemeris/src/engine/highprecision/CalcephKernelProvider.cpp`
  - `libs/skygate-ephemeris/tests/highprecision/CalcephKernelProviderTests.cpp`
  - `.ralph/high-precision-ephemeris-engine/HP-023/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-023/fix.md`
- Tests run after fix:
  - `cmake --build build-ralph`: PASS
  - `ctest --test-dir build-ralph --output-on-failure -R 'skygate-ephemeris-calceph-kernel-provider-tests|skygate-ui-sky-ephemeris-data-manager-tests|skygate-ui-settings-store-tests'`: PASS
  - `ctest --test-dir build-ralph --output-on-failure`: FAIL, 114/115 passed; only the pre-existing unrelated `skygate-ui-qml-main-window-tests` footer popup toolbar assertion failed at `QmlMainWindowTests.cpp(219)`.
- Remaining concerns:
  - The unrelated recurring QML main-window footer popup toolbar failure remains outside HP-023 and is tracked separately.
