## Task
- ID: HP-006B
- Title: Add request-based snapshot compute API

## Status
READY

## Acceptance criteria claimed
- [x] `IEphemerisEngine::compute(const EphemerisRequest&)` added to the public interface
- [x] Simple engine request path implemented using the request epoch and context observer fields
- [x] Existing `SkySnapshot` shape preserved
- [x] Request-based snapshot test added for equivalence with the `SkyContext` path
- [x] Touched C++ files formatted with `clang-format`
- [x] Full build completed in `build-ralph`

## Files changed
- `libs/skygate-ephemeris/include/skygate/ephemeris/IEphemerisEngine.hpp`
- `libs/skygate-ephemeris/src/engine/simple/SimpleEphemerisEngine.cpp`
- `libs/skygate-ephemeris/tests/engine/EphemerisEngineBaselineTests.cpp`

## Important notes
- `ctest --test-dir build-ralph -R '^skygate-ephemeris-engine-baseline-tests$' --output-on-failure` passed.
- `ctest --test-dir build-ralph --output-on-failure` passed 102/103 tests. The only failure was the pre-existing and already tracked HP-051 QML footer popup toolbar regression in `skygate-ui-qml-main-window-tests`.

## Review fixes
- Review verdict addressed: NEEDS_FIX
- Findings addressed:
  - Finding title: Request options are ignored by the simple-engine request path
  - Action: Fixed
  - Notes: The simple-engine request adapter now explicitly consumes request options. Unsupported correction flags or atmospheric refraction requests leave simple-engine coordinates unchanged, mark valid results as degraded, add `CorrectionUnavailable`, and report `NoCorrections` as the applied correction set.
- Files changed during fix pass:
  - `libs/skygate-ephemeris/src/engine/simple/SimpleEphemerisEngine.cpp`
  - `libs/skygate-ephemeris/tests/engine/EphemerisEngineBaselineTests.cpp`
  - `.ralph/high-precision-ephemeris-engine/HP-006B/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-006B/fix.md`
- Tests run after fix:
  - `clang-format -i libs/skygate-ephemeris/src/engine/simple/SimpleEphemerisEngine.cpp libs/skygate-ephemeris/tests/engine/EphemerisEngineBaselineTests.cpp`: PASS
  - `cmake --build build-ralph --target skygate-ephemeris-engine-baseline-tests`: PASS
  - `ctest --test-dir build-ralph -R '^skygate-ephemeris-engine-baseline-tests$' --output-on-failure`: PASS
  - `cmake --build build-ralph`: PASS
  - `git diff --check`: PASS
  - `ctest --test-dir build-ralph --output-on-failure`: FAIL, only the pre-existing HP-051 `skygate-ui-qml-main-window-tests` footer popup regression failed.
- Remaining concerns: Full-suite verification still reports the pre-existing HP-051 QML failure, which is outside HP-006B.
