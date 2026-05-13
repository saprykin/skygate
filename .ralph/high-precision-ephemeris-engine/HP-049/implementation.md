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
