## Task fixed
  - ID: HP-060
  - Title: Wire production ephemeris data acquisition
  - Source: IMPLEMENTATION_PLAN.md

## Review input
  - Review verdict: NEEDS_FIX
  - Review report: .ralph/high-precision-ephemeris-engine/HP-060/review.md
  - Implementation handoff:
    .ralph/high-precision-ephemeris-engine/HP-060/implementation.md

## Summary
Packaged startup now links the ephemeris qrc into the executable and the smoke
test fails if the manifest is not loaded. The production manifest now has real
SHA-256 and size metadata, profile-owned DE441 support assets, and no longer
claims bundled modern data when only metadata is packaged.

## Findings addressed
  - Finding title: Packaged startup does not load the manifest
  - Severity: BLOCKER
  - Action: Fixed
  - File(s): apps/skygate-ui/CMakeLists.txt;
    apps/skygate-ui/tests/PackagedAppSmoke.cmake
  - What changed: Linked the ephemeris qrc into `skygate-ui` and made the
    packaged smoke test require the manifest-loaded log and reject the missing
    manifest warning.
  - Why this resolves the finding: The packaged executable now owns the qrc
    resource object, and CI will fail if startup cannot load `:/ephemeris`.

  - Finding title: Production manifest checksums cannot verify
  - Severity: BLOCKER
  - Action: Fixed
  - File(s): apps/skygate-ui/resources/ephemeris/manifest.json;
    apps/skygate-ui/tests/ephemeris/SkyEphemerisDataManagerTests.cpp
  - What changed: Replaced placeholder checksum values with real SHA-256
    values and uncompressed sizes for each listed production asset. Added a
    test that parses the production manifest and validates checksum metadata.
  - Why this resolves the finding: Staged verification compares the computed
    SHA-256 to manifest metadata, so the manifest now contains exact expected
    values instead of placeholders.

  - Finding title: DE441 profile references assets from another profile
  - Severity: BLOCKER
  - Action: Fixed
  - File(s): apps/skygate-ui/resources/ephemeris/manifest.json;
    apps/skygate-ui/tests/ephemeris/SkyEphemerisDataManagerTests.cpp
  - What changed: Added `de441-long-range` owned support assets for leap
    seconds, Earth orientation, and Delta T data. Added a full long-range
    staged activation test.
  - Why this resolves the finding: Every `de441-long-range` profile asset now
    has matching `profileId`, satisfying both manifest parsing and staged
    verification.

  - Finding title: Bundled modern data is declared but not packaged
  - Severity: BLOCKER
  - Action: Fixed
  - File(s): apps/skygate-ui/resources/ephemeris/manifest.json;
    apps/skygate-ui/src/ephemeris/SkyEphemerisDataManager.cpp;
    apps/skygate-ui/tests/ephemeris/SkyEphemerisDataManagerTests.cpp
  - What changed: Marked the production modern profile unbundled until release
    packaging supplies the actual data files. Tightened bundled fallback
    selection so unbundled profiles are not exposed as fallback data.
  - Why this resolves the finding: Runtime metadata no longer claims that
    missing packaged files are bundled, and fallback lookup cannot silently use
    an unbundled profile.

## Tests run
  - `cmake --build build-ralph --target
    skygate-ui-sky-ephemeris-data-manager-tests skygate-ui`: PASS
  - `ctest --test-dir build-ralph --output-on-failure -R
    'skygate-ui-sky-ephemeris-data-manager-tests|
    skygate-ui-packaged-app-smoke'`:
    PASS
  - `ctest --test-dir build-ralph --output-on-failure -R
    'skygate-ui-acceptance-matrix-tests|
    skygate-ui-context-controller-ephemeris-settings-tests'`:
    PASS
  - `ctest --test-dir build-ralph --output-on-failure`: PASS
    (126 passed, 1 skipped)

## Files changed
  - apps/skygate-ui/CMakeLists.txt
  - apps/skygate-ui/resources/ephemeris/manifest.json
  - apps/skygate-ui/src/ephemeris/SkyEphemerisDataManager.cpp
  - apps/skygate-ui/tests/CMakeLists.txt
  - apps/skygate-ui/tests/PackagedAppSmoke.cmake
  - apps/skygate-ui/tests/ephemeris/SkyEphemerisDataManagerTests.cpp
  - .ralph/high-precision-ephemeris-engine/HP-060/implementation.md
  - .ralph/high-precision-ephemeris-engine/HP-060/fix.md

## Remaining concerns
None.

## Final fixer status
READY_FOR_REVIEW
