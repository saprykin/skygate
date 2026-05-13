## Task fixed
  - ID: HP-023
  - Title: Add CALCEPH kernel loading and selection provider
  - Source: IMPLEMENTATION_PLAN.md / specs/high-precision-ephemeris-engine.md

## Review input
  - Review verdict: NEEDS_FIX
  - Review report: .ralph/high-precision-ephemeris-engine/HP-023/review.md
  - Implementation handoff: .ralph/high-precision-ephemeris-engine/HP-023/implementation.md

## Summary
  Fixed active kernel identity handling so an installed kernel path cannot be reported as a different selected manifest asset. The settings cache now persists installed kernel asset/profile identity, the active snapshot only returns matching kernel assets, and the CALCEPH provider rejects mismatched snapshot assets before opening the file.

## Findings addressed
  - Finding title: Active kernel snapshot can misidentify the selected manifest asset
  - Severity: MAJOR
  - Action: Fixed
  - File(s): apps/skygate-ui/src/settings/SkySettingsStore.hpp; apps/skygate-ui/src/settings/SkySettingsStore.cpp; apps/skygate-ui/src/ephemeris/SkyEphemerisDataManager.cpp; libs/skygate-ephemeris/include/skygate/ephemeris/EphemerisDataSnapshot.hpp; libs/skygate-ephemeris/src/engine/highprecision/CalcephKernelProvider.cpp; apps/skygate-ui/tests/settings/SkySettingsStoreTests.cpp; apps/skygate-ui/tests/ephemeris/SkyEphemerisDataManagerTests.cpp; libs/skygate-ephemeris/tests/highprecision/CalcephKernelProviderTests.cpp
  - What changed: Added installed kernel asset/profile IDs to the persisted ephemeris data cache, carried profile identity on kernel snapshot assets, made the active snapshot return `std::nullopt` for requested asset IDs that do not match the installed kernel, and added provider validation for mismatched snapshot asset/profile IDs.
  - Why this resolves the finding: The provider can no longer treat the single active kernel path as whichever manifest asset was requested. Modern-vs-long-range mismatches are now reported as missing or invalid kernel data before checksum/open handling.

## Tests run
  - `cmake --build build-ralph`: PASS
  - `ctest --test-dir build-ralph --output-on-failure -R 'skygate-ephemeris-calceph-kernel-provider-tests|skygate-ui-sky-ephemeris-data-manager-tests|skygate-ui-settings-store-tests'`: PASS
  - `ctest --test-dir build-ralph --output-on-failure`: FAIL, 114/115 passed; only the pre-existing unrelated `skygate-ui-qml-main-window-tests` footer popup toolbar assertion failed at `apps/skygate-ui/tests/qml/QmlMainWindowTests.cpp(219)`.

## Files changed
  - apps/skygate-ui/src/settings/SkySettingsStore.hpp
  - apps/skygate-ui/src/settings/SkySettingsStore.cpp
  - apps/skygate-ui/src/ephemeris/SkyEphemerisDataManager.cpp
  - apps/skygate-ui/tests/settings/SkySettingsStoreTests.cpp
  - apps/skygate-ui/tests/ephemeris/SkyEphemerisDataManagerTests.cpp
  - libs/skygate-ephemeris/include/skygate/ephemeris/EphemerisDataSnapshot.hpp
  - libs/skygate-ephemeris/src/engine/highprecision/CalcephKernelProvider.cpp
  - libs/skygate-ephemeris/tests/highprecision/CalcephKernelProviderTests.cpp
  - .ralph/high-precision-ephemeris-engine/HP-023/implementation.md
  - .ralph/high-precision-ephemeris-engine/HP-023/fix.md

## Remaining concerns
  The unrelated recurring `skygate-ui-qml-main-window-tests` footer popup toolbar failure remains outside HP-023 and is tracked separately.

## Final fixer status
  READY_FOR_REVIEW
