## Task fixed
  - ID: HP-012
  - Title: Implement UTC, TAI, and TT conversion
  - Source: `IMPLEMENTATION_PLAN.md`

## Review input
  - Review verdict: NEEDS_FIX
  - Review report: `.ralph/high-precision-ephemeris-engine/HP-012/review.md`
  - Implementation handoff: `.ralph/high-precision-ephemeris-engine/HP-012/implementation.md`

## Summary
  Fixed reverse time-scale conversions so they respect leap-second table validity, and made the public civil-to-epoch helper reject UTC leap-second labels instead of silently normalizing them to the next day. `convertCivilDateTime()` remains the supported path for resolving `23:59:60` with table-backed leap-second policy.

## Findings addressed
  - Finding title: Reverse conversions ignore leap-second table validity
  - Severity: MAJOR
  - Action: Fixed
  - File(s): `libs/skygate-ephemeris/src/engine/highprecision/TimeScaleService.cpp`, `libs/skygate-ephemeris/tests/highprecision/TimeScaleServiceTests.cpp`
  - What changed: Added reverse-range validation for TAI epochs by translating the leap-second table UTC validity interval into TAI, then returning failed or degraded lookup status consistently with UTC lookups. Added tests for out-of-range `TAI -> UTC` and `TT -> UTC`.
  - Why this resolves the finding: Reverse conversions can no longer return `Valid` using a stale final table offset after table expiry; `TT -> UTC` delegates through the corrected `TAI -> UTC` path.

  - Finding title: Public civil-to-epoch conversion loses leap-second labels
  - Severity: MAJOR
  - Action: Fixed
  - File(s): `libs/skygate-ephemeris/include/skygate/ephemeris/Types.hpp`, `libs/skygate-ephemeris/src/engine/highprecision/TimeScaleService.cpp`, `libs/skygate-ephemeris/tests/engine/EphemerisApiModelTests.cpp`, `libs/skygate-ephemeris/tests/highprecision/TimeScaleServiceTests.cpp`
  - What changed: Made `astronomicalEpochFromCivilDateTime()` return `std::nullopt` for `second == 60`, while preserving `CivilDateTime` structural validation for UTC `23:59:60`. Updated `convertCivilDateTime()` to synthesize the leap-second epoch from the preceding second before applying table-backed offset validation. Added API-model and service tests for the public helper policy and leap-second service path.
  - Why this resolves the finding: Callers cannot accidentally route a valid leap-second label through the ordinary civil helper and get the post-leap instant; leap-second labels must be resolved through the service API that has the required leap-second table context.

## Tests run
  - `cmake --build build-ralph --target skygate-ephemeris-time-scale-service-tests skygate-ephemeris-api-model-tests` - PASS
  - `ctest --test-dir build-ralph -R 'skygate-ephemeris-(time-scale-service|api-model)-tests' --output-on-failure` - PASS
  - `ctest --test-dir build-ralph -L 'highprecision|unit' --output-on-failure` - PASS
  - `ctest --test-dir build-ralph --output-on-failure` - PASS

## Files changed
  - `libs/skygate-ephemeris/include/skygate/ephemeris/Types.hpp`
  - `libs/skygate-ephemeris/src/engine/highprecision/TimeScaleService.cpp`
  - `libs/skygate-ephemeris/tests/engine/EphemerisApiModelTests.cpp`
  - `libs/skygate-ephemeris/tests/highprecision/TimeScaleServiceTests.cpp`
  - `.ralph/high-precision-ephemeris-engine/HP-012/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-012/fix.md`

## Remaining concerns
  None.

## Final fixer status
  READY_FOR_REVIEW
