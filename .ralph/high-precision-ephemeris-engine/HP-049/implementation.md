## Task
- ID: HP-049
- Title: Restore missing public high-precision API model types

## Status
READY

## Acceptance criteria claimed
- [x] `Types.hpp` audited against HP-004 and the high-precision spec
- [x] Public engine kind, request, options, capabilities, date range, data set, epoch, time scale, and correction flag models added
- [x] Correction flags can represent geometric, astrometric, apparent, and topocentric requests
- [x] Compile/API tests added for defaults, flag combination, request/data-set construction, and exactly two engine kinds
- [x] Existing simple-engine clients still compile
- [x] Existing tests pass

## Files changed
- `libs/skygate-ephemeris/include/skygate/ephemeris/Types.hpp`
- `libs/skygate-ephemeris/tests/engine/EphemerisApiModelTests.cpp`
- `libs/skygate-ephemeris/tests/CMakeLists.txt`
- `.ralph/high-precision-ephemeris-engine/HP-049/implementation.md`

## Important notes
- Configured and built with high precision disabled so optional CALCEPH, zstd, and ERFA dependencies were not required for this public API modeling task.
- Full configured non-UI CTest suite passed in `build-ralph`: 45/45 tests.

## Review fixes
- Review verdict addressed: NEEDS_FIX
- Findings addressed:
  - Finding title: Correction flag `None` can break public header consumers
  - Action: Fixed
  - Notes: Renamed the zero-value correction flag to `NoCorrections`, updated default capability state and flag checks, and added public-header compile coverage for an X11-style `None` macro include order.
- Files changed during fix pass:
  - `libs/skygate-ephemeris/include/skygate/ephemeris/Types.hpp`
  - `libs/skygate-ephemeris/tests/CMakeLists.txt`
  - `libs/skygate-ephemeris/tests/engine/EphemerisApiModelTests.cpp`
  - `libs/skygate-ephemeris/tests/engine/PublicHeaderMacroCompatTests.cpp`
  - `.ralph/high-precision-ephemeris-engine/HP-049/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-049/fix.md`
- Tests run after fix:
  - `cmake --build build-ralph --target skygate-ephemeris-api-model-tests skygate-ephemeris-public-header-macro-compat-tests`: PASS
  - `clang-format --dry-run --Werror libs/skygate-ephemeris/include/skygate/ephemeris/Types.hpp libs/skygate-ephemeris/tests/engine/EphemerisApiModelTests.cpp libs/skygate-ephemeris/tests/engine/PublicHeaderMacroCompatTests.cpp`: PASS
  - `ctest --test-dir build-ralph -R 'skygate-ephemeris-(api-model|public-header-macro-compat)-tests' --output-on-failure`: PASS
  - `ctest --test-dir build-ralph --output-on-failure`: PASS
- Remaining concerns: None for HP-049. A separate HP-050 task was added to `IMPLEMENTATION_PLAN.md` for unrelated catalog public `None` enumerators discovered during the fix pass.
