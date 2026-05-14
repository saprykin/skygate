## Task
- ID: HP-036
- Title: Add kernel and geometric solar-system validation targets

## Status
READY

## Acceptance criteria claimed
- [x] CALCEPH kernel provider validation target is registered only for high-precision builds.
- [x] Geometric solar-system validation target is registered only for high-precision builds.
- [x] Disabled high-precision builds keep clear skipped CTest entries for both validation targets.
- [x] Existing compact Horizons ICRF/no-apparent-correction smoke fixture remains the geometric validation input.
- [x] Existing tests pass in `build-ralph`.

## Files changed
- `libs/skygate-ephemeris/tests/CMakeLists.txt`
- `.ralph/high-precision-ephemeris-engine/HP-036/implementation.md`

## Important notes
- `build-ralph` is configured with `SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=OFF`, so the HP-036 targets were verified as CTest skipped entries in the disabled configuration.
- High-precision enabled execution was not run because this workspace does not have the CALCEPH/zstd/ERFA dependency set installed in `build-ralph`.
