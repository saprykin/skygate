## Task
- ID: HP-006A
- Title: Add engine metadata accessors to `IEphemerisEngine`

## Status
READY

## Acceptance criteria claimed
- [x] `kind()`, `name()`, `capabilities()`, `supportedDateRanges()`, `dataSetInfo()`, and `options()` added to `IEphemerisEngine`
- [x] Simple-engine metadata defaults expose simple engine kind, name, capabilities, options, and built-in data-set provenance
- [x] API tests added for simple-engine metadata defaults
- [x] Existing simple-engine clients still compile in the full UI-enabled build
- [x] Focused ephemeris tests pass

## Files changed
- `libs/skygate-ephemeris/include/skygate/ephemeris/IEphemerisEngine.hpp`
- `libs/skygate-ephemeris/src/engine/simple/SimpleEphemerisEngine.cpp`
- `libs/skygate-ephemeris/tests/engine/EphemerisApiModelTests.cpp`
- `.ralph/high-precision-ephemeris-engine/HP-006A/implementation.md`

## Important notes
- `cmake --build build-ralph -j2` completed successfully.
- Focused verification passed:
  `ctest --test-dir build-ralph --output-on-failure -R 'skygate-ephemeris-(api-model|engine-baseline|engine-fallback|regression)-tests'`.
- Full `ctest --test-dir build-ralph --output-on-failure` ran 103 tests with 102 passing and the already-tracked HP-051 failure in `skygate-ui-qml-main-window-tests::footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()` at `apps/skygate-ui/tests/qml/QmlMainWindowTests.cpp(219)`.
