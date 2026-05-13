## Task
- ID: HP-008B
- Title: Define structured factory result and creation errors

## Status
READY

## Acceptance criteria claimed
- [x] Factory creation status enum added
- [x] Structured creation diagnostics added with stable codes, severity, and non-empty fallback text
- [x] Factory result type added with engine ownership, diagnostics, and success/failure/fallback helpers
- [x] Strict high-precision failure is represented separately from explicit simple-engine fallback
- [x] API tests added for success, strict failure, fallback success with diagnostics, and non-empty diagnostic text
- [x] Touched C++ files formatted with `clang-format`

## Files changed
- `libs/skygate-ephemeris/include/skygate/ephemeris/EphemerisEngineFactory.hpp`
- `libs/skygate-ephemeris/tests/engine/EphemerisApiModelTests.cpp`

## Important notes
- `cmake --build build-ralph --target skygate-ephemeris-api-model-tests -j2` passed.
- `ctest --test-dir build-ralph -R skygate-ephemeris-api-model-tests --output-on-failure` passed.
- `cmake --build build-ralph -j2` passed.
- `ctest --test-dir build-ralph --output-on-failure` ran 104 tests with 103 passing. The only failure was the known unrelated `skygate-ui-qml-main-window-tests` footer popup toolbar assertion tracked by HP-052.
