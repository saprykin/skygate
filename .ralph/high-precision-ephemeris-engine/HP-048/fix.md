## Task fixed
  - ID: HP-048
  - Title: Final acceptance test matrix
  - Source: Spec: Acceptance Criteria

## Review input
  - Review verdict: NEEDS_FIX
  - Review report: `.ralph/high-precision-ephemeris-engine/HP-048/review.md`
  - Implementation handoff:
    `.ralph/high-precision-ephemeris-engine/HP-048/implementation.md`

## Summary
  Expanded the final acceptance matrix so it covers the reviewed gaps: real
  CALCEPH provider wiring when dependencies are enabled, geometric through
  apparent/topocentric correction routing, app-level engine-selection
  persistence, ephemeris data update isolation from catalog state, and optional
  long-range profile selection behavior.

## Findings addressed
  - Finding title: CALCEPH acceptance row uses a fake provider
  - Severity: MAJOR
  - Action: Fixed
  - File(s):
    `libs/skygate-ephemeris/tests/highprecision/`
    `EphemerisAcceptanceMatrixTests.cpp`,
    `libs/skygate-ephemeris/tests/fixtures/ephemeris/kernels/de405s.bsp`
  - What changed: Added a NAIF DE405s BSP fixture and an acceptance row that
    constructs the real `CalcephKernelProvider`, then computes Mars RA/Dec
    through `SolarSystemStateCalculator` when CALCEPH is available.
  - Why this resolves the finding: The high-precision dependency-enabled path
    now opens an actual BSP through the production provider instead of relying
    only on an in-test kernel provider.

  - Finding title: Correction matrix omits apparent and topocentric requests
  - Severity: MAJOR
  - Action: Fixed
  - File(s):
    `libs/skygate-ephemeris/tests/highprecision/`
    `EphemerisAcceptanceMatrixTests.cpp`
  - What changed: Added matrix rows for geometric, astrometric, apparent, and
    apparent/topocentric correction requests. The non-geometric rows route
    through an apparent-place calculator and the topocentric row verifies
    prepared topocentric state and horizontal output.
  - Why this resolves the finding: The matrix now exercises request routing
    beyond light-time and validates requested/applied correction tracking.

  - Finding title: Final matrix misses app-level acceptance rows
  - Severity: MAJOR
  - Action: Fixed
  - File(s): `apps/skygate-ui/tests/app/SkyAcceptanceMatrixTests.cpp`,
    `apps/skygate-ui/tests/CMakeLists.txt`
  - What changed: Added a UI acceptance matrix target covering persisted
    high-precision engine selection across a store reload and ephemeris data
    activation that preserves catalog cache state.
  - Why this resolves the finding: The HP-048 acceptance suite now explicitly
    contains app-level rows for persistence and independent data updates.

  - Finding title: Optional DE441 coverage is metadata-only
  - Severity: MAJOR
  - Action: Fixed
  - File(s):
    `libs/skygate-ephemeris/tests/highprecision/`
    `EphemerisAcceptanceMatrixTests.cpp`,
    `apps/skygate-ui/tests/app/SkyAcceptanceMatrixTests.cpp`
  - What changed: Replaced metadata-only optional long-range coverage with
    bundled and optional profile selection through `CalcephKernelProvider`,
    plus a compute path when CALCEPH is enabled. The app acceptance target also
    verifies update activation behavior.
  - Why this resolves the finding: The final matrix now validates behavior
    through provider/data-manager paths instead of only checking synthetic
    `EphemerisDataSetInfo` records.

## Tests run
  - `cmake --build build-ralph --target`
    `skygate-ephemeris-acceptance-matrix-tests`: PASS
  - `cmake --build build-ralph --target`
    `skygate-ui-acceptance-matrix-tests`: PASS
  - `ctest --test-dir build-ralph -R`
    `"(skygate-ephemeris-acceptance-matrix-tests|`
    `skygate-ui-acceptance-matrix-tests)" --output-on-failure`: PASS
  - `build-ralph/libs/skygate-ephemeris/tests/`
    `skygate-ephemeris-acceptance-matrix-tests -v2`: PASS, 8 passed and 2
    CALCEPH rows skipped in this dependency-disabled build.
  - `ctest --test-dir build-ralph -R`
    `"(skygate-ephemeris-calceph-kernel-provider-tests|`
    `skygate-ephemeris-solar-system-state-calculator-tests|`
    `skygate-ui-context-controller-ephemeris-settings-tests|`
    `skygate-ui-sky-ephemeris-data-manager-tests|`
    `skygate-ui-settings-store-tests)" --output-on-failure`: PASS, with
    `skygate-ephemeris-calceph-kernel-provider-tests` and
    `skygate-ephemeris-solar-system-state-calculator-tests` skipped because
    this build has high-precision dependencies disabled.
  - `ctest --test-dir build-ralph --output-on-failure`: PASS, 127/127 tests,
    with the same two CALCEPH-only tests skipped in this
    dependency-disabled build.

## Files changed
  - `apps/skygate-ui/tests/CMakeLists.txt`
  - `apps/skygate-ui/tests/app/SkyAcceptanceMatrixTests.cpp`
  - `libs/skygate-ephemeris/tests/fixtures/ephemeris/kernels/de405s.bsp`
  - `libs/skygate-ephemeris/tests/highprecision/`
    `EphemerisAcceptanceMatrixTests.cpp`
  - `.ralph/high-precision-ephemeris-engine/HP-048/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-048/fix.md`

## Remaining concerns
  Real CALCEPH rows skip in dependency-disabled builds and should be exercised
  in a `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=ON` build. The multi-GB DE441
  kernel is not included in the repository; the test uses a small deterministic
  BSP fixture to validate the production provider path.

## Final fixer status
  READY_FOR_REVIEW
