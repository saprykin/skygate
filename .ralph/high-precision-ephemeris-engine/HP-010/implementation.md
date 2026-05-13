## Task
- ID: HP-010
- Title: Add astronomical time primitives

## Status
READY

## Acceptance criteria claimed
- [x] `AstronomicalEpoch` remains a public two-part Julian date plus `TimeScale` model
- [x] `CivilDateTime` added with signed astronomical year, calendar fields, and nanosecond subsecond field
- [x] Julian-date normalization helper added
- [x] Civil date/time to astronomical epoch conversion helper added
- [x] Astronomical epoch to civil date/time conversion helper added
- [x] Historical BCE/CE no-year-zero conversion behavior defined through public helpers
- [x] Tests added for normalization, J2000 construction, subsecond round trip, BCE/year-zero behavior, and invalid dates
- [x] Existing `ITimeSource` surface left unchanged

## Files changed
- `libs/skygate-ephemeris/include/skygate/ephemeris/Types.hpp`
- `libs/skygate-ephemeris/tests/engine/EphemerisApiModelTests.cpp`

## Verification
- `clang-format -i libs/skygate-ephemeris/include/skygate/ephemeris/Types.hpp libs/skygate-ephemeris/tests/engine/EphemerisApiModelTests.cpp`
- `cmake --build build-ralph --target skygate-ephemeris-api-model-tests -j 2`
- `ctest --test-dir build-ralph -R '^skygate-ephemeris-api-model-tests$' --output-on-failure`
- `cmake --build build-ralph -j 2`
- `ctest --test-dir build-ralph --output-on-failure`

## Important notes
- Full-suite verification built successfully and ran 107 tests. It reported one unrelated failure in `skygate-ui-qml-main-window-tests`, matching the existing `HP-053` tracked QML footer toolbar issue.
