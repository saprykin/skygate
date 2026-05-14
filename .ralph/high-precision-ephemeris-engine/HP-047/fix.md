## Task fixed
  - ID: HP-047
  - Title: Validate clean-install offline behavior and optional DE441 flow
  - Source: Spec: Data Management; Acceptance Criteria

## Review input
  - Review verdict: NEEDS_FIX
  - Review report:
    `.ralph/high-precision-ephemeris-engine/HP-047/verify.md`
  - Implementation handoff:
    `.ralph/high-precision-ephemeris-engine/HP-047/implementation.md`

## Summary
  Fixed the verifier finding by keeping CALCEPH kernel fallback strict for
  explicit profile and long-range requests. Default selection can still use the
  active snapshot's installed kernel for the HP-047 app acceptance path, but an
  explicit DE441 or profile request now reports the selected kernel as missing
  when that asset is absent.

## Findings addressed
  - Finding title: Explicit kernel profile selection can be silently overridden
  - Severity: MAJOR
  - Action: Fixed
  - File(s):
    `libs/skygate-ephemeris/src/engine/highprecision/CalcephKernelProvider.cpp`,
    `libs/skygate-ephemeris/tests/highprecision/CalcephKernelProviderTests.cpp`,
    `libs/skygate-ephemeris/tests/CMakeLists.txt`
  - What changed: Added an explicit-selection guard around the provider's
    cross-profile snapshot fallback. Added `preferredProfileId` coverage,
    preserved `preferLongRange` missing-asset coverage, documented the default
    active-snapshot fallback behavior in tests, and enabled the provider tests
    in the current non-CALCEPH build because they use a fake runtime.
  - Why this resolves the finding: Explicit `preferredProfileId` and
    `preferLongRange` requests now return `MissingKernelFile` without opening a
    different active kernel, so missing DE441 cannot be masked by a modern
    fallback kernel.

## Tests run
  - `cmake -S . -B build-ralph`: PASS
  - `cmake --build build-ralph --target
    skygate-ephemeris-calceph-kernel-provider-tests
    skygate-ui-acceptance-matrix-tests -j2`: PASS
  - `ctest --test-dir build-ralph --output-on-failure -R
    'skygate-.*-(calceph-kernel-provider|acceptance-matrix)-tests'`:
    PASS
  - `cmake --build build-ralph -j2`: PASS
  - `ctest --test-dir build-ralph --output-on-failure`: PASS, 0 failures out
    of 127 tests. `skygate-ephemeris-solar-system-state-calculator-tests` was
    skipped because this build tree has
    `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=OFF`.

## Files changed
  - `.ralph/high-precision-ephemeris-engine/HP-047/fix.md`
  - `.ralph/high-precision-ephemeris-engine/HP-047/implementation.md`
  - `libs/skygate-ephemeris/src/engine/highprecision/CalcephKernelProvider.cpp`
  - `libs/skygate-ephemeris/tests/CMakeLists.txt`
  - `libs/skygate-ephemeris/tests/highprecision/CalcephKernelProviderTests.cpp`

## Remaining concerns
  None.

## Final fixer status
  READY_FOR_REVIEW
