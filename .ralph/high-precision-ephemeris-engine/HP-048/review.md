## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-048
- Title: Final acceptance test matrix
- Source: Spec: Acceptance Criteria
- Base ref: cfbc6a4fef4e89c446d3137e7036bade62bb2bee
- Head ref: 265cebf5eb655837b527327c21fb015e3dec72d2

## Summary

The implementation adds a new ephemeris acceptance matrix test target and
registers it with CTest. The target builds and passes, and the full
`build-ralph` suite passed locally. However, the matrix does not actually cover
several HP-048 acceptance rows: CALCEPH-backed RA/Dec is represented by an
in-test fake, apparent/topocentric correction requests are not exercised, and
Preferences persistence / independent update flows are absent from the matrix.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: CALCEPH acceptance row uses a fake provider

Severity: MAJOR
File: `libs/skygate-ephemeris/tests/highprecision/EphemerisAcceptanceMatrixTests.cpp`
Lines/functions: 136, 300-327

Problem:
The new CALCEPH-backed RA/Dec acceptance case injects
`AcceptanceKernelProvider`, an in-test fake `ICalcephKernelProvider` with
hard-coded vectors. The asserted provenance string is also produced by that
fake provider. This test would still pass if the real `CalcephKernelProvider`,
CALCEPH runtime wiring, or BSP reads were broken.

Why it matters:
HP-048 requires final matrix coverage that high precision computes
solar-system RA/Dec through CALCEPH-backed kernels. A stub verifies the
calculator interface shape, but it does not verify the integration row the task
is meant to close.

Recommended fix:
Add a matrix row that runs when `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=ON`
and uses the real CALCEPH provider/runtime with the deterministic kernel
fixture or bundled test data. Keep a disabled/skipped placeholder for
simple-only builds if needed, but the high-precision preset should exercise the
real provider path.

### Finding 2: Correction matrix omits apparent and topocentric requests

Severity: MAJOR
File: `libs/skygate-ephemeris/tests/highprecision/EphemerisAcceptanceMatrixTests.cpp`
Lines/functions: 249-255, 298-327

Problem:
The only correction-options case compares `Geometric` and `LightTime`. It does
not request `Astrometric`, `Apparent`, `Topocentric`, or
`ApparentTopocentric`, and the injected dependencies omit the apparent-place
and frame/EOP/refraction path.

Why it matters:
HP-048 verification explicitly requires callers to be able to request
geometric, astrometric, and apparent/topocentric outputs. The current matrix
can pass while apparent/topocentric request routing is absent or broken.

Recommended fix:
Extend the acceptance matrix with rows for geometric, astrometric, apparent,
and apparent/topocentric requests. Assert requested and applied correction
flags, and use dependencies that exercise the apparent-place/topocentric
pipeline instead of the no-op fallback path.

### Finding 3: Final matrix misses app-level acceptance rows

Severity: MAJOR
File: `libs/skygate-ephemeris/tests/highprecision/EphemerisAcceptanceMatrixTests.cpp`
Lines/functions: 249-255

Problem:
The added matrix is ephemeris-library-only. Its slots cover factory behavior,
kernel-stub RA/Dec, degraded metadata, synthetic data-set metadata, fixture
readability, and compute-cache reuse. It does not include matrix rows for
Preferences engine selection persistence across restarts or ephemeris data
updates that leave star/deep-sky catalog state unchanged.

Why it matters:
Those are explicit HP-048 acceptance rows. Existing UI tests cover pieces of
settings persistence and data-manager independence, but this task is the final
acceptance matrix. The new matrix neither references nor aggregates those rows,
so regressions in the end-to-end acceptance workflow are not clearly guarded by
the HP-048 target.

Recommended fix:
Add or register final-matrix coverage for the app-level rows. At minimum,
include a settings/preferences row that changes the selected engine, saves,
reloads, and verifies persistence, plus a data-update row that performs an
ephemeris data activation while catalog settings/cache state are populated and
asserts those catalog values are unchanged.

### Finding 4: Optional DE441 coverage is metadata-only

Severity: MAJOR
File: `libs/skygate-ephemeris/tests/highprecision/EphemerisAcceptanceMatrixTests.cpp`
Lines/functions: 106-116, 216-229, 356-379

Problem:
The bundled-modern and optional-DE441 case constructs synthetic
`EphemerisDataSetInfo` records and checks range metadata. The degraded
long-range case uses a fake calculator that directly returns the expected
warnings. No test computes a long-range request with optional DE441 selected,
and no test exercises clean-install bundled fallback discovery.

Why it matters:
HP-048 requires acceptance coverage that optional DE441 enables long-range
coverage where data permits, and that missing or stale data produces explicit
degraded warnings. Metadata-only and fake-result checks do not prove those
behaviors.

Recommended fix:
Add behavioral coverage for the installed optional long-range profile when the
high-precision data preset is available, and keep a simple-only skip if the
fixture cannot run in dependency-disabled builds. Also add a clean-install
bundled fallback row through the data manager or packaged resource discovery
path.

## Test assessment

The new target builds and passes:
`ctest --test-dir build-ralph -R skygate-ephemeris-acceptance-matrix-tests
--output-on-failure`.

The full suite also passed:
`ctest --test-dir build-ralph --output-on-failure` reported 126/126 tests
passing, with `skygate-ephemeris-calceph-kernel-provider-tests` and
`skygate-ephemeris-solar-system-state-calculator-tests` skipped because this
build has high-precision dependencies disabled.

Coverage is not yet sufficient for HP-048 because several acceptance rows are
absent or represented by stubs instead of the real integration paths.

## Regression risk

Medium

The implementation only adds tests, so runtime regression risk is low. The
review risk is medium because incomplete acceptance coverage can let broken
high-precision integration paths appear fully accepted.

## Out-of-scope observations

Existing UI and data-manager tests cover parts of settings persistence and
ephemeris/catalog separation. They may be reusable for HP-048 if the final
matrix explicitly registers or documents those rows.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
