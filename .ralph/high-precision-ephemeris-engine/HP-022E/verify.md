# Verdict

NEEDS_FIX

# Task verified

- ID: HP-022E
- Title: Add cancellation and failed-update preservation semantics
- Source: IMPLEMENTATION_PLAN.md, specs/high-precision-ephemeris-engine.md
- Base ref: 8c4c6070d8777d931ab5332fecc148c19ba948b2
- Head ref: 3baa08c5b67e35c2e2d7c92cd906ccd7f3529637

# Summary

The implementation adds cancellation callbacks to staged verification and
activation, manager-level update cancellation state, a file-backed staging
transfer API, inactive cache cleanup, and tests for download, verification, and
activation cancellation paths. The two MAJOR review findings are resolved and
the relevant tests pass. One review finding remains open because the added
pre-activation test cancels during verification instead of after verification
succeeds and before the first install write, so the task is not fully ready for
acceptance.

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

- Finding: Download cancellation requirement is not implemented or tested
  - Original severity: MAJOR
  - Closure status: Resolved
  - Notes: `SkyEphemerisDataManager::stageEphemerisUpdateAsset()` now supports manager-level and request-level cancellation during transfer, retains partial staging by default, and has a deterministic cancellation-during-download test.

- Finding: Activation can commit after cancellation is requested
  - Original severity: MAJOR
  - Closure status: Resolved
  - Notes: `activateEphemerisDataAsset()` now checks cancellation after payload copy, after flush, and before commit, canceling the `QSaveFile` on canceled paths. The new low-level activation test covers the post-copy/pre-commit window.

- Finding: Required pre-activation cancellation path lacks coverage
  - Original severity: MINOR
  - Closure status: Still open
  - Notes: The implementation added the post-verification/pre-install cancellation branch, but the claimed manager test returns `EphemerisStagedUpdateVerificationStatus::Canceled`, proving cancellation during verification rather than after successful verification.

# Findings

## Finding 1: Pre-activation cancellation branch is still not covered

Severity: MINOR
File: `apps/skygate-ui/tests/ephemeris/SkyEphemerisDataManagerTests.cpp`
Lines/functions: lines 704-728, `cancellationBeforeActivationPreservesVerifiedStagingAndActiveData`

Problem:
The test intended to cover cancellation after staged update verification and
before the first activation write instead asserts
`EphemerisStagedUpdateVerificationStatus::Canceled`. That means the cancellation
callback fires while `verifyEphemerisStagedUpdateSet()` is still running, and
the manager branch at
`SkyEphemerisDataManager::activateVerifiedStagedUpdateSet()` lines 751-754 is
not exercised.

Why it matters:
The review specifically asked for coverage of the distinct
post-verification/pre-install cancellation path. Without that coverage, a
regression in that branch could still change the active snapshot or cleanup
policy incorrectly while the test suite remains green.

Recommended fix:
Adjust the test so verification completes with
`EphemerisStagedUpdateVerificationStatus::Verified`, then the cancellation
callback returns true on the manager's post-verification cancellation check
before any activation request is made. Assert canceled activation status,
verified verification status, unchanged active snapshot/revision/settings, no
activation output, and the expected staging retention policy.

# Test assessment

Relevant coverage exists in `skygate-ephemeris-data-activation-tests` and
`skygate-ui-sky-ephemeris-data-manager-tests`. The tests cover cancellation
before activation begins, cancellation during staged verification,
cancellation during file-backed download with retained partial staging,
late low-level activation cancellation before commit, activation failure
cleanup, metadata persistence failure cleanup, and active-data preservation.

Commands run:
- `cmake --build build-ralph --target skygate-ephemeris-data-activation-tests skygate-ui-sky-ephemeris-data-manager-tests`
- `ctest --test-dir build-ralph --output-on-failure -R 'skygate-ephemeris-data-activation-tests|skygate-ui-sky-ephemeris-data-manager-tests'`
- `ctest --test-dir build-ralph --output-on-failure`

Results: targeted tests passed, and the full suite passed 120/120. Tests 30 and
31 were skipped by the current CALCEPH-dependent build configuration.

# Regression risk

Low

The remaining issue is a focused coverage gap. The implemented cancellation
logic appears consistent with HP-022E, and the full suite passes, but the
review-requested branch still needs a precise regression test before final
acceptance.

# Out-of-scope observations

- The download staging API currently uses local file copy semantics rather than
  a network transfer backend. That is acceptable for this subtask's deterministic
  cancellation coverage and can be expanded by HP-022F fault-injection work if
  needed.

# Final recommendation

NEEDS_FIX: send back to fixer for another focused fix pass.
