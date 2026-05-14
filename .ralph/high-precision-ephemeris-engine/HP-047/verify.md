# Verdict

PASS

# Task verified

- ID: HP-047
- Title: Validate clean-install offline behavior and optional DE441 flow
- Source: Spec: Data Management; Acceptance Criteria
- Base ref: ef7b2d7ac0ada3d86d244ada6f2ca3aff27b7255
- Head ref: 5b2ceff00099fc665214ee6134e8cc323c4c3de2

# Summary

HP-047 now verifies clean-install bundled modern data, installed modern data,
cache-clear fallback, absent-DE441 out-of-range warnings, and installed-DE441
long-range selection through app-facing acceptance coverage. The latest fix
also closes the previous verifier finding by preserving `MissingKernelFile` for
explicit profile and long-range CALCEPH requests when the requested asset is
absent. Focused and full `build-ralph` tests pass, so the task is ready.

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

- Finding: Out-of-range absent-DE441 behavior is not tested
  - Original severity: MAJOR
  - Closure status: Resolved
  - Notes: Acceptance coverage now creates a strict high-precision engine from
    bundled modern data, requests an epoch outside that range while DE441 is
    absent, and checks out-of-range metadata plus warning text.

- Finding: Bundled fallback and DE441 coverage are metadata-only
  - Original severity: MAJOR
  - Closure status: Resolved
  - Notes: Acceptance coverage now computes modern body states from bundled
    fallback, installed modern data, cache-clear fallback, and an installed
    DE441-only long-range snapshot.

- Finding: Explicit kernel profile selection can be silently overridden
  - Original severity: MAJOR
  - Closure status: Resolved
  - Notes: The CALCEPH provider now only scans other active snapshot kernels
    for default selection. Explicit `preferredProfileId` and `preferLongRange`
    requests keep `MissingKernelFile` when the selected kernel asset is absent,
    with unit coverage for both explicit and default-selection behavior.

# Findings

No findings.

# Test assessment

Relevant provider and acceptance targets built successfully:

- `cmake -S . -B build-ralph`
- `cmake --build build-ralph --target
  skygate-ephemeris-calceph-kernel-provider-tests
  skygate-ui-acceptance-matrix-tests -j2`

Focused tests passed:

- `ctest --test-dir build-ralph --output-on-failure -R
  'skygate-.*-(calceph-kernel-provider|acceptance-matrix)-tests'`

The full build and test suite also passed:

- `cmake --build build-ralph -j2`
- `ctest --test-dir build-ralph --output-on-failure`

CTest reported 127 tests with 0 failures. The only skipped test was
`skygate-ephemeris-solar-system-state-calculator-tests`, because this
`build-ralph` tree has `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=OFF`.

# Regression risk

Low

The remaining changes are focused on app acceptance coverage and CALCEPH kernel
selection semantics. The risky fallback behavior identified by the previous
verifier now has direct unit coverage, and the full suite passes.

# Out-of-scope observations

- A high-precision-enabled build should still run
  `skygate-ephemeris-solar-system-state-calculator-tests` before release
  acceptance, since this local `build-ralph` tree skips it with high precision
  disabled.

# Final recommendation

PASS: ready for final acceptance or merge.
