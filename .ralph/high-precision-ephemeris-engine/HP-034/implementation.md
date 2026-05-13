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

## Review fixes
- Review verdict addressed: NEEDS_FIX
- Findings addressed:
  - Existing smoke CSV remains an LFS pointer
    - Action: Fixed
    - Notes: Re-added `geometric_solar_system_smoke.csv` under the smoke fixture attributes so the staged blob contains fixture content instead of a Git LFS pointer. Added regression coverage that reads the CSV smoke fixture and rejects pointer payload.
  - Kernel artifacts are not covered by the LFS policy
    - Action: Fixed
    - Notes: Added explicit Git LFS attributes for expected ephemeris kernel and compressed kernel archive extensions: `.bsp`, `.spk`, `.bc`, `.bpc`, and `.zst` variants.
  - Angular tolerance helper accepts invalid coordinates
    - Action: Fixed
    - Notes: Added finite-coordinate and tolerance validation before angular comparison. Invalid separations now return NaN and tolerance checks return false for NaN, infinity, or negative tolerance values. Added regression coverage for invalid RA/Dec and tolerance inputs.
- Files changed during fix pass:
  - `.gitattributes`
  - `.ralph/high-precision-ephemeris-engine/HP-034/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-034/fix.md`
  - `libs/skygate-ephemeris/tests/fixtures/ephemeris/geometric_solar_system_smoke.csv`
  - `libs/skygate-ephemeris/tests/highprecision/EphemerisFixtureSupportTests.cpp`
  - `libs/skygate-ephemeris/tests/support/EphemerisFixtureSupport.hpp`
- Tests run after fix:
  - `clang-format -i libs/skygate-ephemeris/tests/support/EphemerisFixtureSupport.hpp libs/skygate-ephemeris/tests/highprecision/EphemerisFixtureSupportTests.cpp` - PASS
  - `git check-attr -a -- libs/skygate-ephemeris/tests/fixtures/ephemeris/geometric_solar_system_smoke.csv libs/skygate-ephemeris/tests/fixtures/ephemeris/geometric_solar_system_smoke.json kernels/de440.bsp libs/skygate-ephemeris/data/kernels/de440.bsp.zst kernels/pck00011.bpc` - PASS
  - `cmake --build build-ralph --target skygate-ephemeris-fixture-support-tests` - PASS
  - `ctest --test-dir build-ralph -R skygate-ephemeris-fixture-support-tests --output-on-failure` - PASS
  - `ctest --test-dir build-ralph --output-on-failure` - PASS, 60/60 tests
- Remaining concerns: None.
