## Task
- ID: HP-038
- Title: Add factory, warning, and fallback validation targets

## Status
READY

## Acceptance criteria claimed
- [x] Factory validation covers simple creation
- [x] Factory validation covers high-precision unavailable fallback
- [x] Factory validation covers strict high-precision failure
- [x] Result validation covers missing DE441 degraded warning behavior
- [x] Result validation covers stale data warnings
- [x] Result validation covers unsupported body, out-of-range request, and failed request status
- [x] Validation target registered with CTest labels
- [x] Existing tests pass

## Files changed
- `libs/skygate-ephemeris/tests/CMakeLists.txt`
- `libs/skygate-ephemeris/tests/highprecision/EphemerisFallbackValidationTests.cpp`

## Important notes
- Full `ctest --test-dir build-ralph --output-on-failure` passed with 60 tests passing and 2 CALCEPH-dependent tests skipped in the current simple-only build configuration.
