## Task
- ID: HP-009
- Title: Add the high-precision engine facade boundary

## Status
READY

## Acceptance criteria claimed
- [x] `HighPrecisionEphemerisEngine` added under `src/engine/highprecision/`
- [x] Facade dependency boundary added for focused high-precision providers and calculators
- [x] Request validation returns structured failed status before calculator dispatch
- [x] Solar-system and catalog-star dispatch covered with fake collaborators
- [x] Unsupported bodies return structured unsupported status instead of silent NaNs
- [x] Option forwarding and result assembly covered by tests
- [x] New high-precision facade test target passes
- [x] Existing full CTest run executed; unrelated UI failures recorded as HP-053

## Files changed
- `libs/skygate-ephemeris/CMakeLists.txt`
- `libs/skygate-ephemeris/src/engine/highprecision/HighPrecisionEphemerisEngine.hpp`
- `libs/skygate-ephemeris/src/engine/highprecision/HighPrecisionEphemerisEngine.cpp`
- `libs/skygate-ephemeris/tests/CMakeLists.txt`
- `libs/skygate-ephemeris/tests/highprecision/HighPrecisionEphemerisEngineTests.cpp`

## Important notes
- The factory still reports real high-precision construction as unavailable; HP-008F owns factory wiring once the later data, time, EOP, and provider dependencies exist.
- `ctest --test-dir build-ralph --output-on-failure` ran after a full build and reported 105/107 tests passing. The two unrelated QML failures were added to `IMPLEMENTATION_PLAN.md` as HP-053 and were not implemented in this task.
