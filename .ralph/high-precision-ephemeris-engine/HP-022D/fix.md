## Task fixed
  - ID: HP-022D
  - Title: Atomically activate verified ephemeris data
  - Source: IMPLEMENTATION_PLAN.md

## Review input
  - Review verdict: NEEDS_FIX
  - Review report: .ralph/high-precision-ephemeris-engine/HP-022D/review.md
  - Implementation handoff: .ralph/high-precision-ephemeris-engine/HP-022D/implementation.md

## Summary
  Fixed the active-data preservation hole for same-revision activations and exposed activated leap-second and Delta T assets through the installed data snapshot.

## Findings addressed
  - Finding title: Activation can overwrite active files before the update set commits
  - Severity: BLOCKER
  - Action: Fixed
  - File(s): apps/skygate-ui/src/ephemeris/SkyEphemerisDataManager.cpp, apps/skygate-ui/tests/ephemeris/SkyEphemerisDataManagerTests.cpp
  - What changed: Same-revision activation now avoids any cache root that contains currently active asset paths and writes the attempted update into a non-active activation root. The regression test forces a later asset activation failure after an earlier asset writes under that non-active root.
  - Why this resolves the finding: A failed or interrupted same-token activation no longer writes into paths referenced by the active snapshot or persisted settings, so the old active files remain intact even after partial activation.

  - Finding title: Activated leap-second and Delta T assets are not exposed as active data
  - Severity: MAJOR
  - Action: Fixed
  - File(s): apps/skygate-ui/src/settings/SkySettingsStore.hpp, apps/skygate-ui/src/settings/SkySettingsStore.cpp, apps/skygate-ui/src/ephemeris/SkyEphemerisDataManager.cpp, apps/skygate-ui/tests/settings/SkySettingsStoreTests.cpp, apps/skygate-ui/tests/ephemeris/SkyEphemerisDataManagerTests.cpp
  - What changed: Added persisted active paths for leap-second and Delta T assets, populated them during activation, included them in snapshot normalization/change detection/missing-path checks, and loaded both text assets from `SkyActiveEphemerisDataSnapshot`.
  - Why this resolves the finding: Consumers using `leapSecondTableAsset()` and `deltaTDataAsset()` can now read the installed payloads after successful activation instead of seeing missing data while versions are reported as installed.

## Tests run
  - `clang-format -i apps/skygate-ui/src/settings/SkySettingsStore.hpp apps/skygate-ui/src/settings/SkySettingsStore.cpp apps/skygate-ui/src/ephemeris/SkyEphemerisDataManager.cpp apps/skygate-ui/tests/settings/SkySettingsStoreTests.cpp apps/skygate-ui/tests/ephemeris/SkyEphemerisDataManagerTests.cpp`: PASS
  - `cmake --build build-ralph --target skygate-ui-sky-ephemeris-data-manager-tests`: PASS
  - `cmake --build build-ralph --target skygate-ui-settings-store-tests`: PASS
  - `ctest --test-dir build-ralph -R '^skygate-ui-sky-ephemeris-data-manager-tests$' --output-on-failure`: PASS
  - `ctest --test-dir build-ralph -R '^skygate-ui-settings-store-tests$' --output-on-failure`: PASS
  - `cmake --build build-ralph --target skygate-ephemeris-data-activation-tests && ctest --test-dir build-ralph -R '^skygate-ephemeris-data-activation-tests$' --output-on-failure`: PASS
  - `git diff --check`: PASS

## Files changed
  - apps/skygate-ui/src/ephemeris/SkyEphemerisDataManager.cpp
  - apps/skygate-ui/src/settings/SkySettingsStore.hpp
  - apps/skygate-ui/src/settings/SkySettingsStore.cpp
  - apps/skygate-ui/tests/ephemeris/SkyEphemerisDataManagerTests.cpp
  - apps/skygate-ui/tests/settings/SkySettingsStoreTests.cpp
  - .ralph/high-precision-ephemeris-engine/HP-022D/implementation.md
  - .ralph/high-precision-ephemeris-engine/HP-022D/fix.md

## Remaining concerns
  None.

## Final fixer status
  READY_FOR_REVIEW
