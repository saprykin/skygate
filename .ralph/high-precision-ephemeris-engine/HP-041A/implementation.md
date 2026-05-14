## Task
- ID: HP-041A
- Title: Build app-level ephemeris request context

## Status
READY

## Acceptance criteria claimed
- [x] App-level ephemeris request context added to `SkyContextController`
- [x] Request context includes current `SkyContext`, selected engine options,
  active ephemeris data snapshot, data revision, and catalog revision
- [x] UTC controller time is converted into an `AstronomicalEpoch`
- [x] Restored correction options and refraction settings flow into requests
- [x] Tests added for defaults, restored settings, observer/time propagation,
  data revision metadata, and BCE epoch conversion
- [x] Touched C++ files formatted with `clang-format`
- [x] Existing tests pass

## Files changed
- `apps/skygate-ui/src/app/SkyContextController.hpp`
- `apps/skygate-ui/src/app/SkyContextController.cpp`
- `apps/skygate-ui/tests/app/SkyContextControllerEphemerisSettingsTests.cpp`

## Important notes
- Full verification passed with `ctest --test-dir build-ralph
  --output-on-failure`.
- The existing `build-ralph` configuration skipped the two CALCEPH-dependent
  tests:
  `skygate-ephemeris-calceph-kernel-provider-tests` and
  `skygate-ephemeris-solar-system-state-calculator-tests`.
