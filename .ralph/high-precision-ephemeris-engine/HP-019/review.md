## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-019
- Title: Add zstd archive handling and first-use activation
- Source: IMPLEMENTATION_PLAN.md; spec/high-precision-ephemeris-engine.md
- Base ref: e5c06d75403452c941f882c9670ffd72a49252ab
- Head ref: d314d9e2120a9ad0f0b3a82c42b9a9eb9fca2c72

## Summary

The implementation adds a public activation API, zstd streaming decompression, checksum and size validation, cache idempotency, Qt-resource rejection for large kernels, and focused activation tests. The main data path works in the current `build-ralph` environment and the full configured test suite passes. However, cache promotion is not atomic and can delete a previously valid active asset if the final rename fails, so the implementation needs a fix before merge.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: Cache promotion can delete an existing active asset on failure

Severity: BLOCKER
File: `libs/skygate-ephemeris/src/engine/highprecision/EphemerisDataActivation.cpp`
Lines/functions: lines 518-523, `activateEphemerisDataAsset`

Problem:
After successfully writing and validating the temporary activation file, the code calls `removeFileIfPresent(targetPathString)` before `QFile::rename(temporaryPath, targetPathString)`. If the rename then fails, the function returns `IoError` and removes the temporary file, but the previously active cache file has already been deleted.

Why it matters:
HP-019 is adding first-use activation into a writable application data cache. A failed activation must not make an already valid cache unusable. This can lose the user's active ephemeris asset on common filesystem errors such as permissions, cross-device behavior, antivirus/file-lock interference, or an unexpected target-path state. It also contradicts the diagnostic text that claims an atomic promotion.

Recommended fix:
Promote the replacement without deleting the current active file first. Use an actual atomic/safe replacement mechanism, such as writing via `QSaveFile` to the final target and committing, or a platform-aware replace operation that either leaves the old file intact on failure or provides rollback. Add a regression test where a valid existing active file remains present when replacement promotion fails.

## Test assessment

`skygate-ephemeris-data-activation-tests` covers valid zstd activation, corrupt archive failure, checksum mismatch failure, idempotent activation, and large-kernel Qt-resource rejection. The focused activation test passes, and `ctest --test-dir build-ralph --output-on-failure` passes 56/56 tests.

The tests do not currently assert the expected-size mismatch path independently from checksum mismatch, and the corrupt/checksum failure cases do not assert that no final cache file or temporary file remains. Those gaps should be tightened when fixing the promotion behavior.

## Regression risk

Medium

The new API is isolated to ephemeris data activation, but it writes into persistent app data cache paths. The current promotion failure mode can remove a previously valid cache asset, so the risk is data availability rather than broad runtime breakage.

## Out-of-scope observations

- The review prompt references `specs/high-precision-ephemeris-engine.md`, but this checkout stores the document at `spec/high-precision-ephemeris-engine.md`.
- `EphemerisDataActivation.cpp` dynamically loads zstd by library name even though high-precision builds already require `zstd::libzstd`; this may deserve a follow-up decision on whether zstd activation should use the linked dependency or remain optional at runtime.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
