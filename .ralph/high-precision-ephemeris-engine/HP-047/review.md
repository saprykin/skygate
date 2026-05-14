## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-047
- Title: Validate clean-install offline behavior and optional DE441 flow
- Source: Spec: Data Management; Acceptance Criteria
- Base ref: ef7b2d7ac0ada3d86d244ada6f2ca3aff27b7255
- Head ref: f29121cee618f5d96d2328954c1f45c51851fa38

## Summary

HP-047 added acceptance-matrix coverage for ephemeris data manager metadata
flows: empty bundled fallback status, staged modern activation, optional DE441
profile activation, cache clear, and catalog-state isolation. Those tests pass,
but they do not verify the central HP-047 acceptance behavior: clean-install
offline high-precision operation from bundled modern data, explicit degraded
warnings for out-of-range dates when DE441 is absent, or actual long-range
calculation/selection when DE441 is installed.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: Out-of-range absent-DE441 behavior is not tested

Severity: MAJOR
File: `apps/skygate-ui/tests/app/SkyAcceptanceMatrixTests.cpp`
Lines/functions: lines 218-242,
`cleanInstallOfflineModernDataActivatesAndClearReturnsToBundled`

Problem:
HP-047 requires verification that dates outside the bundled kernel range
degrade with an explicit warning when DE441 is absent. The new test only
activates the modern profile and asserts that `longRangeKernelStatusText()` is
`"Not installed"` and that the active snapshot has no `de441-kernel`. It never
requests an out-of-range date, never invokes the high-precision engine, and
never asserts degraded status, warning codes, or tooltip/display warning text.

Why it matters:
The test can pass even if the application fails to surface the required
out-of-range degraded warning for users without DE441. That warning path is
explicit in the HP-047 task and in the Data Management and Acceptance Criteria
sections of the spec.

Recommended fix:
Add an integration or acceptance smoke test that configures high precision with
only bundled modern data, requests a date outside that bundled range, and
asserts the result/status path exposes degraded or out-of-range metadata plus
the user-visible warning text. Reuse existing high-precision acceptance helpers
where possible, but cover the app-facing warning/status path required by
HP-047.

### Finding 2: Bundled fallback and DE441 coverage are metadata-only

Severity: MAJOR
File: `apps/skygate-ui/tests/app/SkyAcceptanceMatrixTests.cpp`
Lines/functions: lines 244-279,
`cleanInstallOfflineModernDataActivatesAndClearReturnsToBundled`,
`optionalLongRangeProfileActivationSelectsDe441Kernel`

Problem:
The clean-install and cache-clear checks assert that fallback status is
`"Bundled fallback"`, but after clearing they explicitly assert that the
fallback snapshot does not provide `de440s-kernel`. The optional-DE441 test
only verifies that staged metadata exposes a fake `de441-kernel`. Neither test
proves that a clean install can run high precision in the bundled modern range
without network access, that clearing installed data returns to usable bundled
modern data, or that installing DE441 enables a requested long-range
calculation where data permits.

Why it matters:
HP-047 is a packaging/acceptance task. A metadata-only test does not catch a
broken packaged/bundled data path, nor a broken app-to-engine DE441 selection
path. The current assertions would still pass if no bundled kernel were usable
after cache clear, which is the opposite of the required offline fallback
behavior.

Recommended fix:
Extend the acceptance coverage so the bundled fallback snapshot can provide the
packaged modern kernel data needed by the high-precision path, then compute a
modern-range body state before and after clearing installed data. For DE441,
activate the long-range profile and verify the app/engine selection path
actually uses the long-range data for a long-range request, not just that the
manager stored the profile metadata.

## Test assessment

The changed `skygate-ui-acceptance-matrix-tests` target is registered and
runnable. It now covers useful `SkyEphemerisDataManager` metadata transitions,
but it does not cover HP-047's required bundled offline compute path,
absent-DE441 degraded warning path, or installed-DE441 long-range compute path.

Tests run:

- `cmake --build build-ralph --target skygate-ui-acceptance-matrix-tests -j2`
  passed.
- `ctest --test-dir build-ralph --output-on-failure -R
  '^skygate-ui-acceptance-matrix-tests$'` passed.
- `ctest --test-dir build-ralph --output-on-failure -R <acceptance-matrix
  pair>` passed for the ephemeris and UI acceptance matrix tests.
- `ctest --test-dir build-ralph --output-on-failure` passed 127/127 tests,
  with the existing CALCEPH-backed provider/calculator tests skipped in this
  high-precision-disabled build tree.

## Regression risk

Low

The implementation only changes acceptance tests and does not modify
production behavior. The risk is acceptance false confidence rather than a
runtime regression.

## Out-of-scope observations

- The current `build-ralph` tree has
  `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=OFF`, so CALCEPH-backed provider and
  solar-system calculator tests are skip stubs in the full test run.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
