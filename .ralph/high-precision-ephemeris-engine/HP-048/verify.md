# Verdict

PASS

# Task verified

- ID: HP-048
- Title: Final acceptance test matrix
- Source: Spec: Acceptance Criteria
- Base ref: cfbc6a4fef4e89c446d3137e7036bade62bb2bee
- Head ref: ef552785cf902eb963fc57abfe59a91cf3ce595a

# Summary

HP-048 added final acceptance matrix coverage for ephemeris engine selection,
CALCEPH-backed solar-system integration, correction routing, degraded fallback
metadata, bundled and optional profile selection, deterministic fixtures,
full-frame cache reuse, persisted app settings, and ephemeris data update
isolation from catalog state. The review raised four MAJOR coverage gaps, and
the fix pass added focused ephemeris and UI acceptance rows for each gap. The
relevant targets and full `build-ralph` suite pass in the current
dependency-disabled configuration, with the expected CALCEPH-only skips.

# Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read review report
- [x] Read fixer report, if present
- [x] Read relevant specs
- [x] Inspected git history
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

# Review/fix closure

- Finding: CALCEPH acceptance row uses a fake provider
  - Original severity: MAJOR
  - Closure status: Resolved
  - Notes: A real `CalcephKernelProvider` row now opens the DE405s BSP fixture
    and computes Mars RA/Dec when CALCEPH is enabled. It skips only in builds
    where CALCEPH support is unavailable.

- Finding: Correction matrix omits apparent and topocentric requests
  - Original severity: MAJOR
  - Closure status: Resolved
  - Notes: The matrix now covers geometric, astrometric, apparent, and
    apparent/topocentric requests, including apparent-place routing,
    topocentric preparation, and horizontal output for topocentric requests.

- Finding: Final matrix misses app-level acceptance rows
  - Original severity: MAJOR
  - Closure status: Resolved
  - Notes: `skygate-ui-acceptance-matrix-tests` now covers persisted
    high-precision engine settings across a store reload and ephemeris update
    activation while preserving catalog cache state.

- Finding: Optional DE441 coverage is metadata-only
  - Original severity: MAJOR
  - Closure status: Resolved
  - Notes: The matrix now validates bundled and optional long-range profile
    selection through `CalcephKernelProvider` metadata and a compute path when
    CALCEPH is enabled. The repository uses the small deterministic DE405s
    fixture as a DE441 substitute rather than storing the multi-gigabyte DE441
    kernel.

# Findings

No findings.

# Test assessment

Relevant focused tests were run:

- Acceptance matrix targets: PASS, 2/2.
  Command: `ctest --test-dir build-ralph -R` with
  `skygate-ephemeris-acceptance-matrix-tests` and
  `skygate-ui-acceptance-matrix-tests`.
- Related CALCEPH, calculator, settings, and data-manager targets: PASS, 5/5
  with the two CALCEPH-only tests skipped.
  Command: `ctest --test-dir build-ralph -R` with the related target names.
- `ctest --test-dir build-ralph --output-on-failure`: PASS, 127/127 with the
  two CALCEPH-only tests skipped.

Coverage now matches the HP-048 acceptance rows for this build configuration.
The remaining external validation gap is exercising the real CALCEPH acceptance
rows in a `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=ON` build.

# Regression risk

Low

The task adds acceptance tests and a deterministic BSP fixture. The only source
changes are test registration and test code. Full-suite regression testing
passed in `build-ralph`.

# Out-of-scope observations

- The real CALCEPH acceptance rows remain skipped in the current
  dependency-disabled `build-ralph` configuration. They should be exercised in a
  high-precision-enabled CI or release-validation build.

# Final recommendation

PASS: ready for final acceptance or merge.
