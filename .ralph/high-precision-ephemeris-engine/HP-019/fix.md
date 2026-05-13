## Task fixed
  - ID: HP-019
  - Title: Add zstd archive handling and first-use activation
  - Source: IMPLEMENTATION_PLAN.md; spec/high-precision-ephemeris-engine.md

## Review input
  - Review verdict: NEEDS_FIX
  - Review report: .ralph/high-precision-ephemeris-engine/HP-019/review.md
  - Implementation handoff: .ralph/high-precision-ephemeris-engine/HP-019/implementation.md

## Summary
  Fixed cache activation promotion so a failed final commit cannot delete an existing active ephemeris data asset. The activation path now writes through `QSaveFile`, validates size and SHA-256 before commit, and cancels the temporary write on validation or decompression failures.

## Findings addressed
  - Finding title: Cache promotion can delete an existing active asset on failure
  - Severity: BLOCKER
  - Action: Fixed
  - File(s): libs/skygate-ephemeris/src/engine/highprecision/EphemerisDataActivation.cpp; libs/skygate-ephemeris/tests/highprecision/EphemerisDataActivationTests.cpp
  - What changed: Replaced explicit temporary-file delete/rename promotion with `QSaveFile` commit semantics and added regression tests for failed replacement preserving existing cache contents, expected-size mismatch cleanup, corrupt archive cleanup, and checksum mismatch cleanup.
  - Why this resolves the finding: `QSaveFile` commits only after the activated bytes are fully written and verified, and failed writes or commits leave the previous final-path file intact instead of removing it before promotion.

## Tests run
  - `cmake --build build-ralph --target skygate-ephemeris-data-activation-tests` - PASS
  - `ctest --test-dir build-ralph --output-on-failure -R skygate-ephemeris-data-activation-tests` - PASS
  - `ctest --test-dir build-ralph --output-on-failure` - PASS

## Files changed
  - libs/skygate-ephemeris/src/engine/highprecision/EphemerisDataActivation.cpp
  - libs/skygate-ephemeris/tests/highprecision/EphemerisDataActivationTests.cpp
  - .ralph/high-precision-ephemeris-engine/HP-019/implementation.md
  - .ralph/high-precision-ephemeris-engine/HP-019/fix.md

## Remaining concerns
  None.

## Final fixer status
  READY_FOR_REVIEW
