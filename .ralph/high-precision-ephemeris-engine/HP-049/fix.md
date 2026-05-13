## Task fixed
  - ID: HP-049
  - Title: Restore missing public high-precision API model types
  - Source: IMPLEMENTATION_PLAN.md

## Review input
  - Review verdict: NEEDS_FIX
  - Review report: `.ralph/high-precision-ephemeris-engine/HP-049/review.md`
  - Implementation handoff: `.ralph/high-precision-ephemeris-engine/HP-049/implementation.md`

## Summary
  Renamed the zero-value high-precision correction flag from `None` to `NoCorrections` so public header consumers are not broken by platform headers that define a `None` macro. Added compile coverage that defines `None` before including the public ephemeris engine interface.

## Findings addressed
  - Finding title: Correction flag `None` can break public header consumers
  - Severity: MAJOR
  - Action: Fixed
  - File(s): `libs/skygate-ephemeris/include/skygate/ephemeris/Types.hpp`, `libs/skygate-ephemeris/tests/CMakeLists.txt`, `libs/skygate-ephemeris/tests/engine/EphemerisApiModelTests.cpp`, `libs/skygate-ephemeris/tests/engine/PublicHeaderMacroCompatTests.cpp`
  - What changed: Replaced `EphemerisCorrectionFlags::None` with `EphemerisCorrectionFlags::NoCorrections`, updated `hasCorrectionFlag` and default capability initialization, updated existing API tests, and registered a public-header macro compatibility test.
  - Why this resolves the finding: `NoCorrections` does not collide with the X11 `None` macro, and the new test verifies that `IEphemerisEngine.hpp` remains usable when `None` is already defined before the public header is parsed.

## Tests run
  - `cmake --build build-ralph --target skygate-ephemeris-api-model-tests skygate-ephemeris-public-header-macro-compat-tests`: PASS
  - `clang-format --dry-run --Werror libs/skygate-ephemeris/include/skygate/ephemeris/Types.hpp libs/skygate-ephemeris/tests/engine/EphemerisApiModelTests.cpp libs/skygate-ephemeris/tests/engine/PublicHeaderMacroCompatTests.cpp`: PASS
  - `ctest --test-dir build-ralph -R 'skygate-ephemeris-(api-model|public-header-macro-compat)-tests' --output-on-failure`: PASS
  - `ctest --test-dir build-ralph --output-on-failure`: PASS

## Files changed
  - `libs/skygate-ephemeris/include/skygate/ephemeris/Types.hpp`
  - `libs/skygate-ephemeris/tests/CMakeLists.txt`
  - `libs/skygate-ephemeris/tests/engine/EphemerisApiModelTests.cpp`
  - `libs/skygate-ephemeris/tests/engine/PublicHeaderMacroCompatTests.cpp`
  - `.ralph/high-precision-ephemeris-engine/HP-049/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-049/fix.md`

## Remaining concerns
  None.

## Final fixer status
  READY_FOR_REVIEW
