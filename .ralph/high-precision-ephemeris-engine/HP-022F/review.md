## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-022F
- Title: Add update-flow test harness and fault injection
- Source: IMPLEMENTATION_PLAN.md, high-precision ephemeris engine spec
- Base ref: 9685b1d7563e06c252bb7022c1c4aced37021c02
- Head ref: 67a045488c7081d7413a67bcad84db37df8b27fe

## Summary

The implementation adds a reusable test helper and four update-flow tests for
successful activation, restart after partial download, checksum failure, and
activation cancellation. The tests pass in the current debug build, but the
harness is not robust across build configurations and the HP-022F-required
interrupted-install/fault-injection coverage is still missing from the new
harness tests.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: Source payload write is hidden behind Q_ASSERT

Severity: MAJOR
File: `apps/skygate-ui/tests/ephemeris/SkyEphemerisDataManagerTests.cpp`
Lines/functions: `EphemerisUpdateFlowHarness::stageKernelPayload`

Problem:
`stageKernelPayload()` writes the source asset with
`Q_ASSERT(writeFile(sourcePath, payload));`. In non-debug Qt builds,
`Q_ASSERT` may compile out the expression, so the source payload is never
created before the staging API is exercised.

Why it matters:
The HP-022F harness is meant to deterministically exercise update-flow staging.
This helper becomes build-configuration dependent: the same test can pass in a
debug build and fail with `MissingSource`, or skip the intended staging path, in
a release-style test build.

Recommended fix:
Execute the write unconditionally and assert the result with a test assertion,
for example by replacing the `Q_ASSERT(writeFile(...))` call with a `QVERIFY`
or equivalent helper that is not compiled out.

### Finding 2: Harness does not cover interrupted install failure

Severity: MAJOR
File: `apps/skygate-ui/tests/ephemeris/SkyEphemerisDataManagerTests.cpp`
Lines/functions: harness activation-cancellation test

Problem:
HP-022F explicitly requires the reusable harness to cover interrupted install
and all supported injected failures. The new harness tests cover checksum
failure and activation cancellation, but they do not exercise a non-cancelled
interrupted install or activation I/O failure through the reusable harness.
Older non-harness tests cover some activation failure behavior, but this task
specifically asks for the update-flow harness and fault-injection coverage.

Why it matters:
The most important atomic-activation guarantee is that active data and settings
survive an install failure after verification succeeds. Without this harness
case, HP-022F does not prove the reusable flow can inject and validate that
failure mode.

Recommended fix:
Add a harness-based test that verifies staged data, injects an activation
failure or interrupted install after activation begins, and asserts the active
snapshot, settings, revision, and partial cache cleanup remain correct.

## Test assessment

The added tests are registered in
`skygate-ui-sky-ephemeris-data-manager-tests`. I ran the targeted test and the
full `build-ralph` CTest suite successfully:

- `ctest --test-dir build-ralph --output-on-failure -R
  '^skygate-ui-sky-ephemeris-data-manager-tests$'`
- `ctest --test-dir build-ralph --output-on-failure`

The full suite passed 120/120 test entries in this build configuration, with
the existing CALCEPH-dependent tests skipped. Coverage is still incomplete for
HP-022F because the new harness does not include the required interrupted
install/fault-injection scenario, and one helper is not deterministic outside
debug builds.

## Regression risk

Medium

The implementation only changes tests, so product runtime risk is low. The risk
is medium for the task because the added harness can fail or miss the intended
path in non-debug builds, and it leaves a required activation-failure scenario
uncovered.

## Out-of-scope observations

No out-of-scope observations.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
