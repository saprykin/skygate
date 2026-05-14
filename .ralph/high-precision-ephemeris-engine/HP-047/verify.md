# Verdict

NEEDS_FIX

# Task verified

- ID: HP-047
- Title: Validate clean-install offline behavior and optional DE441 flow
- Source: Spec: Data Management; Acceptance Criteria
- Base ref: ef7b2d7ac0ada3d86d244ada6f2ca3aff27b7255
- Head ref: 8a74fcfeae8ad5fd3e09b1291bc97f1960f9de7c

# Summary

HP-047 now covers the app-facing high-precision compute path for bundled
modern fallback data, installed modern data, cache-clear fallback, absent-DE441
out-of-range warnings, and optional DE441 long-range activation. The prior
review findings are resolved at the acceptance-test level. However, the fix
broadened CALCEPH kernel-provider fallback so explicit profile or long-range
selection can silently use another active kernel when the requested one is
absent. That weakens DE441 selection semantics and is not covered in the
current simple-only `build-ralph` configuration because the relevant provider
test is skipped.

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
  - Notes: The acceptance test now creates a strict high-precision engine from
    bundled modern data, requests an epoch outside that range while DE441 is
    absent, and checks out-of-range metadata plus warning text.

- Finding: Bundled fallback and DE441 coverage are metadata-only
  - Original severity: MAJOR
  - Closure status: Resolved
  - Notes: The acceptance test now computes modern body states from bundled
    fallback, installed modern data, cache-clear fallback, and an installed
    DE441-only long-range snapshot.

# Findings

## Finding 1: Explicit kernel profile selection can be silently overridden

Severity: MAJOR
File: `libs/skygate-ephemeris/src/engine/highprecision/CalcephKernelProvider.cpp`
Lines/functions: lines 246-262, `CalcephKernelProvider::CalcephKernelProvider`

Problem:
When the selected profile's kernel is absent from the active snapshot, the
provider now scans every manifest profile and switches to the first kernel the
snapshot exposes. That fallback also runs when selection was explicit through
`CalcephKernelSelectionOptions::preferredProfileId` or `preferLongRange`.
Therefore a caller requesting DE441/long-range data can receive a modern
bundled kernel instead of a missing-kernel failure.

Why it matters:
HP-047 is specifically about optional DE441 absence and activation semantics.
Silently substituting a different kernel can mask an absent DE441 install and
make strict long-range/profile requests appear ready with the wrong data. The
existing provider unit test for this behavior is not run in this `build-ralph`
configuration because `skygate-ephemeris-calceph-kernel-provider-tests` is
skipped when high precision is disabled.

Recommended fix:
Only fall back to the active snapshot's available kernel when the caller did
not explicitly request a profile or long-range selection, or add a dedicated
selection mode for "use active installed profile." Preserve
`MissingKernelFile` for explicit `preferredProfileId` and `preferLongRange`
requests when the requested asset is absent, then add or enable coverage for
that case in a high-precision-capable test configuration.

# Test assessment

The focused affected targets built successfully:

- `cmake --build build-ralph --target skygate-ui-acceptance-matrix-tests
  skygate-ui-qml-preferences-catalog-tests -j2`

Focused tests passed:

- `ctest --test-dir build-ralph --output-on-failure -R`
  `^(skygate-ui-acceptance-matrix-tests|skygate-ui-qml-preferences-catalog-tests)$`

The full `build-ralph` build and test suite also passed:

- `cmake --build build-ralph -j2`
- `ctest --test-dir build-ralph --output-on-failure`

CTest reported 127/127 tests passed, with
`skygate-ephemeris-calceph-kernel-provider-tests` and
`skygate-ephemeris-solar-system-state-calculator-tests` skipped in this
high-precision-disabled build tree.

# Regression risk

Medium

The app acceptance path is substantially better covered now, but the provider
selection fallback affects shared high-precision data selection semantics and
can hide wrong-kernel activation under explicit long-range/profile requests.

# Out-of-scope observations

- The current `build-ralph` tree has
  `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=OFF`, so CALCEPH-backed provider and
  solar-system calculator tests are skip stubs in the full test run.

# Final recommendation

NEEDS_FIX: send back to fixer for another focused fix pass.
