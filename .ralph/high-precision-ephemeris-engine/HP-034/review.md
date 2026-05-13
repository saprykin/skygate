## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-034
- Title: Add ephemeris fixture infrastructure and LFS policy
- Source: Spec: Testing Requirements; Reference Fixture Policy
- Base ref: 7bf0c3b72e914011385471c6f41e592e5ddf16b4
- Head ref: e3a54edb2f321815e067e28da7a282b804b4324f

## Summary

The implementation adds a JSON ephemeris smoke fixture, a fixture-loading helper, angular comparison utilities, tests, and a smoke-fixture LFS exception. The new target builds and passes, and the full configured `build-ralph` suite passes locally. However, the LFS policy is incomplete for the task, an existing smoke fixture remains committed as an LFS pointer despite the new non-LFS smoke rule, and the angular tolerance helper can incorrectly accept invalid NaN/inf coordinates.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: Existing smoke CSV remains an LFS pointer

Severity: MAJOR
File: `.gitattributes`
Lines/functions: line 2; `libs/skygate-ephemeris/tests/fixtures/ephemeris/geometric_solar_system_smoke.csv`

Problem:
The new `.gitattributes` exception makes `libs/skygate-ephemeris/tests/fixtures/ephemeris/*_smoke.*` non-LFS going forward, but the already tracked `geometric_solar_system_smoke.csv` blob in `HEAD` is still a Git LFS pointer. `git show HEAD:libs/skygate-ephemeris/tests/fixtures/ephemeris/geometric_solar_system_smoke.csv` returns pointer text, not the 373-byte CSV content present in the working tree.

Why it matters:
HP-034 requires at least one small smoke fixture to remain available in a normal checkout. The existing solar-system state test reads this CSV unconditionally, so a checkout without LFS hydration can still see pointer payload instead of smoke data and fail before the high-precision validation path is usable.

Recommended fix:
After the `.gitattributes` exception, renormalize and re-add `libs/skygate-ephemeris/tests/fixtures/ephemeris/geometric_solar_system_smoke.csv` so the committed blob contains the actual small CSV content. Consider adding a test or check that verifies committed smoke fixtures are not LFS pointer payloads.

### Finding 2: Kernel artifacts are not covered by the LFS policy

Severity: MAJOR
File: `.gitattributes`
Lines/functions: lines 1-2

Problem:
The LFS rules only cover `libs/skygate-ephemeris/tests/fixtures/**`, with a smoke-fixture exception. HP-034 explicitly requires LFS rules so large fixtures and kernels use LFS, and the spec also calls out bundled kernel archives when they must live in the repository. Kernel/SPK/archive assets outside the test fixture tree are not protected by the current attributes.

Why it matters:
Future kernel or compressed kernel assets could be committed as normal Git blobs, which is exactly the repository-size failure the task is meant to prevent.

Recommended fix:
Add explicit LFS patterns for the expected ephemeris kernel/archive locations and extensions, such as `.bsp`, `.spk`, `.bc`, `.bpc`, and compressed kernel archives like `.bsp.zst`, while keeping the non-LFS smoke fixture exception after the broad fixture rule.

### Finding 3: Angular tolerance helper accepts invalid coordinates

Severity: MAJOR
File: `libs/skygate-ephemeris/tests/support/EphemerisFixtureSupport.hpp`
Lines/functions: `angularSeparationDegrees`, `isWithinAngularTolerance`

Problem:
`angularSeparationDegrees()` does not validate input coordinates. If an actual or expected RA/Dec contains NaN or infinity, the trig operations produce NaN, then the `std::max(0.0, haversine)` clamp can turn that into `0.0`, producing zero separation. `isWithinAngularTolerance()` can therefore return true for invalid computed coordinates.

Why it matters:
The helper is intended for reference fixture validation. High-precision engine failures and unsupported states often surface as NaN coordinates, and those must fail fixture comparisons rather than pass as zero angular error.

Recommended fix:
Check all input RA/Dec values and the tolerance with `std::isfinite()` before computing or comparing. Return false from `isWithinAngularTolerance()` for invalid inputs, and add tests covering NaN/inf actual coordinates.

## Test assessment

The implementation adds `skygate-ephemeris-fixture-support-tests` covering parser success, malformed JSON, incomplete metadata, LFS pointer detection, smoke fixture availability, and RA wraparound angular comparison. The added target builds and passes. The full configured suite also passes: `ctest --test-dir build-ralph --output-on-failure` passed 60/60 tests.

Missing coverage: invalid actual RA/Dec values in angular comparisons, committed non-LFS status for all smoke fixtures, and LFS policy coverage for kernel artifacts.

## Regression risk

Medium

The new helper is test-only, but it can mask future high-precision validation failures if invalid coordinates compare as within tolerance. The incomplete LFS policy can also leave normal checkouts unable to run smoke validation reliably.

## Out-of-scope observations

Existing CSV fixture loaders in `SolarSystemStateCalculatorTests.cpp` use `Q_ASSERT` for fixture parsing and do not detect LFS pointer payloads. That was pre-existing, but it makes the non-LFS smoke fixture policy more important.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
