## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-022E
- Title: Add cancellation and failed-update preservation semantics
- Source: IMPLEMENTATION_PLAN.md, specs/high-precision-ephemeris-engine.md
- Base ref: 8c4c6070d8777d931ab5332fecc148c19ba948b2
- Head ref: c558dfc20ff77e86a1fc5e9f9653fda1ec9a9272

## Summary

The implementation adds cancellation callbacks to staged verification, asset
activation, and manager-level activation cleanup, with tests for several
verification and activation cancellation paths. It does not fully satisfy
HP-022E because transfer/download cancellation is not implemented or tested, and
the low-level activation path can still promote a staged file if cancellation is
requested after streaming completes but before the atomic commit.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: Download cancellation requirement is not implemented or tested

Severity: MAJOR
File: `apps/skygate-ui/src/ephemeris/SkyEphemerisDataManager.cpp`
Lines/functions: `requestUpdateCancellation`, `activateVerifiedStagedUpdateSet`

Problem:
HP-022E requires safe cancellation of active transfers and tests for
cancellation during download. The implementation only adds cancellation checks
around staged-set verification and activation. I found no ephemeris download or
transfer path in the HP-022E diff, and the new tests cover cancellation before
verification, during verification, and during activation, but not during
download.

Why it matters:
An update canceled while assets are still being transferred can still leave
undefined staging behavior relative to the task requirements. The acceptance
criteria for active-transfer cancellation and cancellation-during-download
coverage are therefore not met.

Recommended fix:
Wire the same cancellation request surface into the ephemeris download/staging
operation introduced by the update flow, define whether partially downloaded
files are retained or cleaned by policy, and add a deterministic test that
cancels during transfer and verifies active snapshot/revision preservation.

### Finding 2: Activation can commit after cancellation is requested

Severity: MAJOR
File: `libs/skygate-ephemeris/src/engine/highprecision/EphemerisDataActivation.cpp`
Lines/functions: `activateEphemerisDataAsset`, lines 716-737

Problem:
`activateEphemerisDataAsset()` polls cancellation while streaming bytes, but it
does not poll again after streaming succeeds and before `targetFile.flush()`,
checksum validation, and `targetFile.commit()`. If cancellation is requested in
that window, the function can still atomically promote the staged file and
return `Activated`.

Why it matters:
HP-022E requires cancellation during install jobs and unchanged active data on
cancellation. The manager may clean its inactive revision root after a
late-canceled activation, but the public activation primitive itself can still
publish a replacement after cancellation. This is especially risky for callers
that use the activation helper directly or for any future path where the target
root is already active.

Recommended fix:
Check `request.cancellationRequested` after payload streaming and before every
state-changing promotion step, at minimum before `flush()` and before
`commit()`. On cancellation, call `targetFile.cancelWriting()` and return
`EphemerisDataActivationStatus::Canceled`. Add a test where the callback becomes
true after payload copy but before commit.

### Finding 3: Required pre-activation cancellation path lacks coverage

Severity: MINOR
File: `apps/skygate-ui/tests/ephemeris/SkyEphemerisDataManagerTests.cpp`
Lines/functions: `cancellationBeforeVerificationPreservesActiveDataAndRetainsStaging`, `cancellationDuringActivationCleansPartialCacheAndPreservesActiveData`

Problem:
HP-022E calls for cancellation before activation. The implementation has an
explicit post-verification/pre-install cancellation branch in
`SkyEphemerisDataManager::activateVerifiedStagedUpdateSet`, but the added tests
do not exercise it. The similarly named test cancels before verification starts,
and the activation test cancels after the first activated asset exists.

Why it matters:
The untested branch has distinct behavior from both verification cancellation
and partial activation cleanup. In particular, it should preserve verified
staging according to policy without producing activation output or changing the
active revision.

Recommended fix:
Add a manager-level test where verification completes, the cancellation callback
then starts returning true before the first asset activation, and the test
asserts canceled status, unchanged active snapshot/revision/settings, and the
expected staging retention/cleanup policy.

## Test assessment

Relevant tests were added in
`skygate-ephemeris-data-activation-tests` and
`skygate-ui-sky-ephemeris-data-manager-tests`. They cover cancellation before
verification, during verification, during activation after cache output begins,
partial activation cleanup, retained staging, and active revision/settings
preservation. Missing coverage remains for cancellation during ephemeris
download/transfer, the post-verification/pre-install branch, and the activation
commit-window case.

Commands run:
- `cmake --build build-ralph --target skygate-ephemeris-data-activation-tests skygate-ui-sky-ephemeris-data-manager-tests`
- `ctest --test-dir build-ralph --output-on-failure -R 'skygate-ephemeris-data-activation-tests|skygate-ui-sky-ephemeris-data-manager-tests'`
- `ctest --test-dir build-ralph --output-on-failure`

Results: targeted tests passed, and the full suite passed 120/120. Tests 30 and
31 were skipped by the current CALCEPH-dependent build configuration.

## Regression risk

Medium

The changes touch update activation control flow and cleanup paths for data
that will eventually include large external assets. Existing tests pass, but the
missing transfer cancellation and late activation cancellation windows leave
observable semantics incomplete.

## Out-of-scope observations

- `requestUpdateCancellation()` is sticky until explicitly cleared. That may be
  intentional, but restart behavior after cancellation should be covered by
  HP-022F or documented for callers.
- The already-active checksum path does not poll cancellation while hashing an
  existing active file, so cancellation responsiveness may be poor for large
  files.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
