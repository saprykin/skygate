## Task
- ID: HP-048
- Title: Final acceptance test matrix

## Status
READY

## Acceptance criteria claimed
- [x] Final acceptance matrix test target added
- [x] Engine selection strict/fallback behavior covered
- [x] CALCEPH-provider-backed solar-system RA/Dec covered
- [x] Correction option application covered
- [x] Missing DE441-style long-range fallback warnings covered
- [x] Bundled modern and optional long-range data metadata covered
- [x] Deterministic Horizons fixture readability covered
- [x] Full-frame computation cache reuse covered
- [x] Existing tests pass

## Files changed
- `libs/skygate-ephemeris/tests/CMakeLists.txt`
- `libs/skygate-ephemeris/tests/highprecision/`
  `EphemerisAcceptanceMatrixTests.cpp`

## Important notes
- Full `ctest --test-dir build-ralph --output-on-failure` passed 126/126.
- CALCEPH-only kernel targets were skipped in this build configuration because
  high-precision dependencies are disabled.

## Review fixes
- Review verdict addressed: NEEDS_FIX
- Findings addressed:
  - Finding title: CALCEPH acceptance row uses a fake provider
    - Action: Fixed
    - Notes: Added a NAIF DE405s BSP fixture and a real
      `CalcephKernelProvider` acceptance row. The row skips only when the build
      does not enable CALCEPH.
  - Finding title: Correction matrix omits apparent and topocentric requests
    - Action: Fixed
    - Notes: Added geometric, astrometric, apparent, and
      apparent/topocentric rows that route through an injected apparent-place
      calculator and topocentric preparation dependencies.
  - Finding title: Final matrix misses app-level acceptance rows
    - Action: Fixed
    - Notes: Added a UI acceptance matrix target covering persisted engine
      selection and ephemeris data activation that preserves catalog cache
      state.
  - Finding title: Optional DE441 coverage is metadata-only
    - Action: Fixed
    - Notes: Replaced metadata-only coverage with provider-selection behavior
      for bundled and optional long-range profiles plus a compute path when
      CALCEPH is enabled. The multi-GB DE441 kernel remains outside the repo.
- Files changed during fix pass:
  - `apps/skygate-ui/tests/CMakeLists.txt`
  - `apps/skygate-ui/tests/app/SkyAcceptanceMatrixTests.cpp`
  - `libs/skygate-ephemeris/tests/fixtures/ephemeris/kernels/de405s.bsp`
  - `libs/skygate-ephemeris/tests/highprecision/`
    `EphemerisAcceptanceMatrixTests.cpp`
  - `.ralph/high-precision-ephemeris-engine/HP-048/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-048/fix.md`
- Tests run after fix:
  - `cmake --build build-ralph --target`
    `skygate-ephemeris-acceptance-matrix-tests`: PASS
  - `cmake --build build-ralph --target`
    `skygate-ui-acceptance-matrix-tests`: PASS
  - `ctest --test-dir build-ralph -R`
    `"(skygate-ephemeris-acceptance-matrix-tests|`
    `skygate-ui-acceptance-matrix-tests)" --output-on-failure`: PASS
  - `build-ralph/libs/skygate-ephemeris/tests/`
    `skygate-ephemeris-acceptance-matrix-tests -v2`: PASS, 8 passed and 2
    CALCEPH rows skipped in this dependency-disabled build
  - `ctest --test-dir build-ralph -R`
    `"(skygate-ephemeris-calceph-kernel-provider-tests|`
    `skygate-ephemeris-solar-system-state-calculator-tests|`
    `skygate-ui-context-controller-ephemeris-settings-tests|`
    `skygate-ui-sky-ephemeris-data-manager-tests|`
    `skygate-ui-settings-store-tests)" --output-on-failure`: PASS, with the
    two CALCEPH-only tests skipped in this dependency-disabled build
  - `ctest --test-dir build-ralph --output-on-failure`: PASS, 127/127, with
    the two CALCEPH-only tests skipped in this dependency-disabled build
- Remaining concerns: Real CALCEPH acceptance rows require configuring
  `build-ralph` with `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=ON`; the current
  build keeps those dependency-only paths skipped.
