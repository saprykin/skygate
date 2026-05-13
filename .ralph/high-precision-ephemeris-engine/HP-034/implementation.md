## Task
- ID: HP-034
- Title: Add ephemeris fixture infrastructure and LFS policy

## Status
READY

## Acceptance criteria claimed
- [x] Ephemeris fixture directory contains a metadata-complete smoke fixture
- [x] Fixture loading helper validates required metadata and expected RA/Dec fields
- [x] Shared angular tolerance utilities added for RA/Dec comparisons
- [x] Parser success, parser failure, metadata completeness, and LFS-pointer detection tests added
- [x] Git LFS rules keep large fixtures under LFS while allowing small smoke fixtures to stay available in a normal checkout
- [x] Existing tests pass

## Files changed
- `.gitattributes`
- `libs/skygate-ephemeris/tests/CMakeLists.txt`
- `libs/skygate-ephemeris/tests/support/EphemerisFixtureSupport.hpp`
- `libs/skygate-ephemeris/tests/highprecision/EphemerisFixtureSupportTests.cpp`
- `libs/skygate-ephemeris/tests/fixtures/ephemeris/geometric_solar_system_smoke.json`

## Important notes
- Verification run: `ctest --test-dir build-ralph --output-on-failure` passed 60/60 tests.
