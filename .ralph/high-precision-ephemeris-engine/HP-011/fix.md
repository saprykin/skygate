## Task fixed
  - ID: HP-011
  - Title: Add leap-second table loading
  - Source: Spec: Time Model; Data Management; Testing Requirements

## Review input
  - Review verdict: NEEDS_FIX
  - Review report: `.ralph/high-precision-ephemeris-engine/HP-011/review.md`
  - Implementation handoff: `.ralph/high-precision-ephemeris-engine/HP-011/implementation.md`

## Summary
  Fixed malformed leap-second expiration metadata handling so a recognized
  `#@ expires` line with an invalid date no longer loads as an available table.
  Added regression coverage for malformed expiration metadata.

## Findings addressed
  - Finding title: Invalid expiration metadata is accepted as a usable table
  - Severity: MAJOR
  - Action: Fixed
  - File(s): `libs/skygate-ephemeris/src/engine/highprecision/LeapSecondProvider.cpp`, `libs/skygate-ephemeris/tests/highprecision/LeapSecondProviderTests.cpp`
  - What changed: Metadata parsing now distinguishes recognized metadata keys
    from unknown metadata. Invalid `#@ expires` dates return a malformed load
    result with no provider, no expiration epoch, and no validity range. A test
    now asserts that malformed expiration metadata is rejected.
  - Why this resolves the finding: Stale-table detection and validity-range
    reporting depend on expiration metadata. Rejecting malformed expiration
    data prevents the loader from silently substituting the final leap-second
    row as a freshness boundary while reporting normal availability.

## Tests run
  - `cmake --build build-ralph --target skygate-ephemeris-leap-second-provider-tests`: PASS
  - `ctest --test-dir build-ralph -R '^skygate-ephemeris-leap-second-provider-tests$' --output-on-failure`: PASS
  - `ctest --test-dir build-ralph -R 'skygate-ephemeris-(api-model|leap-second-provider)-tests' --output-on-failure`: PASS
  - `ctest --test-dir build-ralph --output-on-failure`: PASS

## Files changed
  - `.ralph/high-precision-ephemeris-engine/HP-011/fix.md`
  - `.ralph/high-precision-ephemeris-engine/HP-011/implementation.md`
  - `libs/skygate-ephemeris/src/engine/highprecision/LeapSecondProvider.cpp`
  - `libs/skygate-ephemeris/tests/highprecision/LeapSecondProviderTests.cpp`

## Remaining concerns
  None.

## Final fixer status
  READY_FOR_REVIEW
