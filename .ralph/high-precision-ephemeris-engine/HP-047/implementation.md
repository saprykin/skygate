## Task
- ID: HP-047
- Title: Validate clean-install offline behavior and optional DE441 flow

## Status
READY

## Acceptance criteria claimed
- [x] Clean-install bundled fallback starts without installed ephemeris data
- [x] Offline modern ephemeris data activation is covered by acceptance tests
- [x] Absent DE441 state remains visible as not installed
- [x] Optional DE441 profile activation exposes the long-range kernel
- [x] Clearing ephemeris data returns to bundled fallback behavior
- [x] Existing full `build-ralph` test suite passes

## Files changed
- `apps/skygate-ui/tests/app/SkyAcceptanceMatrixTests.cpp`

## Important notes
- `ctest --test-dir build-ralph --output-on-failure` passed 127/127 tests.
- `skygate-ephemeris-calceph-kernel-provider-tests` and
  `skygate-ephemeris-solar-system-state-calculator-tests` were skipped because
  the existing `build-ralph` tree has
  `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=OFF`.

## Review fixes

- Review verdict addressed: NEEDS_FIX
- Findings addressed:
  - Out-of-range absent-DE441 behavior is not tested
    - Action: Fixed
    - Notes: Added app acceptance coverage that builds a strict
      high-precision engine from bundled modern fallback data, requests a date
      outside the bundled modern range while DE441 is absent, and verifies the
      out-of-range result metadata plus user-visible warning text.
  - Bundled fallback and DE441 coverage are metadata-only
    - Action: Fixed
    - Notes: Added bundled fallback data exposure for configured app
      resources, high-precision compute checks for clean install, installed
      modern data, cache-clear fallback, and optional DE441 long-range
      activation. Also fixed provider selection so a DE441-only active
      snapshot can drive high-precision engine creation.
- Files changed during fix pass:
  - `apps/skygate-ui/src/app/SkyContextController.cpp`
  - `apps/skygate-ui/src/ephemeris/SkyEphemerisDataManager.cpp`
  - `apps/skygate-ui/src/ephemeris/SkyEphemerisDataManager.hpp`
  - `apps/skygate-ui/tests/CMakeLists.txt`
  - `apps/skygate-ui/tests/app/SkyAcceptanceMatrixTests.cpp`
  - `apps/skygate-ui/tests/qml/QmlPreferencesCatalogTests.cpp`
  - `libs/skygate-ephemeris/src/engine/highprecision/CalcephKernelProvider.cpp`
- Tests run after fix:
  - `cmake --build build-ralph --target skygate-ui-acceptance-matrix-tests
    skygate-ui-qml-preferences-catalog-tests -j2` - PASS
  - `ctest --test-dir build-ralph --output-on-failure -R
    'skygate-ui-(acceptance-matrix|qml-preferences-catalog)-tests'`
    - PASS
  - `cmake --build build-ralph -j2` - PASS
  - `ctest --test-dir build-ralph --output-on-failure` - PASS, 127/127 tests
    passed with two existing high-precision CALCEPH-backed tests skipped
    because this build tree has high precision disabled.
- Remaining concerns: None.

## Review fixes

- Review verdict addressed: NEEDS_FIX
- Findings addressed:
  - Explicit kernel profile selection can be silently overridden
    - Action: Fixed
    - Notes: Restricted CALCEPH provider snapshot-kernel fallback to default
      selection only. Explicit `preferredProfileId` and `preferLongRange`
      requests now preserve `MissingKernelFile` when the selected asset is not
      active, instead of silently opening another profile's kernel.
- Files changed during fix pass:
  - `libs/skygate-ephemeris/src/engine/highprecision/CalcephKernelProvider.cpp`
  - `libs/skygate-ephemeris/tests/CMakeLists.txt`
  - `libs/skygate-ephemeris/tests/highprecision/CalcephKernelProviderTests.cpp`
  - `.ralph/high-precision-ephemeris-engine/HP-047/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-047/fix.md`
- Tests run after fix:
  - `cmake -S . -B build-ralph` - PASS
  - `cmake --build build-ralph --target
    skygate-ephemeris-calceph-kernel-provider-tests
    skygate-ui-acceptance-matrix-tests -j2` - PASS
  - `ctest --test-dir build-ralph --output-on-failure -R
    'skygate-.*-(calceph-kernel-provider|acceptance-matrix)-tests'`
    - PASS
  - `cmake --build build-ralph -j2` - PASS
  - `ctest --test-dir build-ralph --output-on-failure` - PASS, 0 failures out
    of 127 tests with `skygate-ephemeris-solar-system-state-calculator-tests`
    skipped because this build tree has high precision disabled.
- Remaining concerns: None.
