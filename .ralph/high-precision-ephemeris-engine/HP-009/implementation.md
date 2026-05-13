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

## Review fixes
- Review verdict addressed: NEEDS_FIX
- Findings addressed:
  - Finding title: Option-forwarding test does not distinguish request options from engine defaults
    - Action: Fixed
    - Notes: The facade option-forwarding test now constructs the engine with different default correction flags from the per-request flags, then verifies calculators, apparent-place processing, result building, and returned metadata use the request-specific flags.
  - Finding title: High-precision facade tests are not discoverable through the highprecision label
    - Action: Fixed
    - Notes: The high-precision facade Qt test target now registers with `LABELS "unit;highprecision"`, and `ctest -L highprecision -N` discovers it.
- Files changed during fix pass:
  - `libs/skygate-ephemeris/tests/CMakeLists.txt`
  - `libs/skygate-ephemeris/tests/highprecision/HighPrecisionEphemerisEngineTests.cpp`
  - `.ralph/high-precision-ephemeris-engine/HP-009/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-009/fix.md`
- Tests run after fix:
  - `clang-format -i libs/skygate-ephemeris/tests/highprecision/HighPrecisionEphemerisEngineTests.cpp`: PASS
  - `cmake --build build-ralph --target skygate-ephemeris-highprecision-engine-tests`: PASS
  - `ctest --test-dir build-ralph -R skygate-ephemeris-highprecision-engine-tests --output-on-failure`: PASS
  - `ctest --test-dir build-ralph -L highprecision -N`: PASS
  - `ctest --test-dir build-ralph -L highprecision --output-on-failure`: PASS
  - `git diff --check`: PASS
  - `ctest --test-dir build-ralph --output-on-failure`: FAIL, 106/107 passed; remaining failure is `skygate-ui-qml-main-window-tests`, the known unrelated HP-053 footer popup toolbar toggle issue.
- Remaining concerns:
  - Full-suite UI failure remains in `skygate-ui-qml-main-window-tests` and is tracked separately as HP-053.
