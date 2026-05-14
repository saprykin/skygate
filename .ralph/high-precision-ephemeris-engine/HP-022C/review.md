## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-022C
- Title: Verify staged update sets before activation
- Source: IMPLEMENTATION_PLAN.md
- Base ref: dae5044affe662e13d4522a3c20e196538b3c4f6
- Head ref: 64ce4c9d45d2ae87be23cbd825b48c6928106654

## Summary

The implementation adds an explicit `verifyEphemerisStagedUpdateSet()` API and extends data activation tests for successful staged verification, checksum failure, wrong component kind, missing staged files, malformed path metadata, corrupt compressed data, and unsupported profile handling. The payload and component-kind checks are useful, but the API does not let callers specify expected versions or validity ranges, so HP-022C's version/range verification requirement is not actually enforceable before activation.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: Expected versions and validity ranges are not verified

Severity: MAJOR
File: `libs/skygate-ephemeris/include/skygate/ephemeris/EphemerisDataActivation.hpp`
Lines/functions: lines 115-126, `EphemerisStagedUpdateVerificationRequest`; `verifyEphemerisStagedUpdateSet`

Problem:
HP-022C requires staged verification to check versions and validity ranges before activation. The new request model only lets callers provide expected component asset IDs and kinds. The implementation checks that each manifest asset has a non-empty version and an ordered finite range, but it never compares those values against caller-selected expected versions or expected validity coverage. A staged set with an internally well-formed but stale EOP version, wrong leap-second table version, or too-narrow kernel validity range can still return `Verified` if the payload checksum matches its manifest.

Why it matters:
This makes the explicit pre-activation verification surface unable to reject one of the main classes of bad update sets HP-022C is meant to catch: complete and checksummed data that is not the requested or required data set.

Recommended fix:
Extend `EphemerisStagedUpdateVerificationRequest::ExpectedComponent` or add a parallel policy structure with expected version and validity-range requirements. During `verifyEphemerisStagedUpdateSet()`, compare staged manifest metadata against those requirements and reject mismatches. Add regression tests for version mismatch and insufficient/incorrect validity range coverage.

### Finding 2: Standalone metadata validation accepts incomplete validity-range labels

Severity: MINOR
File: `libs/skygate-ephemeris/src/engine/highprecision/EphemerisDataActivation.cpp`
Lines/functions: lines 442-470, `validateAssetMetadata`

Problem:
`validateAssetMetadata()` validates that the date range epochs are finite and ordered, but it does not validate required date-range metadata such as `id` and `displayName`. The manifest parser requires those fields when reading JSON, but the new standalone verifier takes an already materialized manifest and is also meant to reject malformed staged metadata.

Why it matters:
If staged metadata is recovered or assembled outside the JSON parser, malformed range metadata can pass the new verification API. That weakens the claimed malformed-metadata coverage for the standalone verification surface.

Recommended fix:
Have `validateAssetMetadata()` also require non-empty `asset.validityRange.id` and `asset.validityRange.displayName`, matching `parseDateRange()`, and add a focused test that clears one of those fields and expects `MalformedMetadata`.

## Test assessment

The focused target `skygate-ephemeris-data-activation-tests` passes and covers several important staged verification cases: success, checksum mismatch, wrong component kind, missing staged file, unsafe path metadata, corrupt zstd payload, and unsupported profile. Coverage is still missing for expected version mismatch and validity-range mismatch, which are required by HP-022C.

Full `ctest --test-dir build-ralph --output-on-failure` ran 120 tests with 119 passing and one unrelated recurring failure in `skygate-ui-qml-main-window-tests::footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()` at `apps/skygate-ui/tests/qml/QmlMainWindowTests.cpp(219)`.

## Regression risk

Medium

The new API is additive and the focused test target passes, but accepting stale or wrong validity metadata before activation is a meaningful data-management correctness risk.

## Out-of-scope observations

- The full test suite still has the known unrelated QML footer popup toolbar failure noted by the implementer.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
