## Task fixed
  - ID: HP-047
  - Title: Validate clean-install offline behavior and optional DE441 flow
  - Source: Spec: Data Management; Acceptance Criteria

## Review input
  - Review verdict: NEEDS_FIX
  - Review report: `.ralph/high-precision-ephemeris-engine/HP-047/review.md`
  - Implementation handoff:
    `.ralph/high-precision-ephemeris-engine/HP-047/implementation.md`

## Summary
  Fixed HP-047 acceptance coverage so it exercises app-facing high-precision
  creation and computation, not just ephemeris data metadata. Bundled fallback
  data can now expose configured packaged modern kernel resources, installed
  DE441-only snapshots can drive provider selection, and the acceptance matrix
  verifies clean-install modern compute, absent-DE441 out-of-range warnings,
  cache-clear fallback compute, and optional DE441 long-range compute.

## Findings addressed
  - Finding title: Out-of-range absent-DE441 behavior is not tested
  - Severity: MAJOR
  - Action: Fixed
  - File(s):
    `apps/skygate-ui/tests/app/SkyAcceptanceMatrixTests.cpp`,
    `apps/skygate-ui/src/ephemeris/SkyEphemerisDataManager.cpp`,
    `apps/skygate-ui/src/ephemeris/SkyEphemerisDataManager.hpp`,
    `apps/skygate-ui/src/app/SkyContextController.cpp`
  - What changed: Added bundled fallback data exposure and an acceptance check
    that creates a strict high-precision engine from clean-install bundled
    modern data, requests an epoch outside that modern range while DE441 is
    absent, and verifies out-of-range metadata plus warning text.
  - Why this resolves the finding: The test now invokes the high-precision
    app/engine path and verifies the explicit user-facing warning path for an
    absent-DE441 long-range request.

  - Finding title: Bundled fallback and DE441 coverage are metadata-only
  - Severity: MAJOR
  - Action: Fixed
  - File(s):
    `apps/skygate-ui/tests/app/SkyAcceptanceMatrixTests.cpp`,
    `apps/skygate-ui/tests/qml/QmlPreferencesCatalogTests.cpp`,
    `apps/skygate-ui/tests/CMakeLists.txt`,
    `apps/skygate-ui/src/ephemeris/SkyEphemerisDataManager.cpp`,
    `apps/skygate-ui/src/ephemeris/SkyEphemerisDataManager.hpp`,
    `apps/skygate-ui/src/app/SkyContextController.cpp`,
    `libs/skygate-ephemeris/src/engine/highprecision/CalcephKernelProvider.cpp`
  - What changed: Extended acceptance coverage to compute a modern body state
    from clean-install bundled fallback data, installed modern data, and
    bundled fallback after cache clear. Added optional DE441 activation coverage
    that creates a high-precision engine and computes a long-range state.
    Updated provider selection to use the active snapshot's available kernel
    profile when the default bundled profile is not active.
  - Why this resolves the finding: The acceptance tests now prove that the
    bundled fallback and optional DE441 flows are usable by the compute path,
    not merely stored in metadata.

## Tests run
  - `cmake --build build-ralph --target skygate-ui-acceptance-matrix-tests
    skygate-ui-qml-preferences-catalog-tests -j2`: PASS
  - `ctest --test-dir build-ralph --output-on-failure -R
    '^(skygate-ui-acceptance-matrix-tests|skygate-ui-qml-preferences-catalog-tests)$'`:
    PASS
  - `cmake --build build-ralph -j2`: PASS
  - `ctest --test-dir build-ralph --output-on-failure`: PASS

## Files changed
  - `.ralph/high-precision-ephemeris-engine/HP-047/fix.md`
  - `.ralph/high-precision-ephemeris-engine/HP-047/implementation.md`
  - `apps/skygate-ui/src/app/SkyContextController.cpp`
  - `apps/skygate-ui/src/ephemeris/SkyEphemerisDataManager.cpp`
  - `apps/skygate-ui/src/ephemeris/SkyEphemerisDataManager.hpp`
  - `apps/skygate-ui/tests/CMakeLists.txt`
  - `apps/skygate-ui/tests/app/SkyAcceptanceMatrixTests.cpp`
  - `apps/skygate-ui/tests/qml/QmlPreferencesCatalogTests.cpp`
  - `libs/skygate-ephemeris/src/engine/highprecision/CalcephKernelProvider.cpp`

## Remaining concerns
  None.

## Final fixer status
  READY_FOR_REVIEW
