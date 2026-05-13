## Task
- ID: HP-008A
- Title: Define factory request and fallback policy model

## Status
READY

## Acceptance criteria claimed
- [x] Factory request type added with engine kind, catalog bodies, options, data manifest/snapshot, time-scale service, Earth-orientation provider, fallback policy, and diagnostics sink fields
- [x] Fallback policy model distinguishes strict high precision from explicit simple-engine fallback
- [x] API-only opaque handles used for future data, time-scale, EOP, and diagnostics dependencies
- [x] Compile/API tests added for default, simple, and high-precision factory request construction
- [x] Focused ephemeris API/interface tests pass

## Files changed
- `libs/skygate-ephemeris/include/skygate/ephemeris/EphemerisEngineFactory.hpp`
- `libs/skygate-ephemeris/tests/engine/EphemerisApiModelTests.cpp`

## Important notes
- `cmake --build build-ralph --target skygate-ephemeris-api-model-tests` passed.
- `ctest --test-dir build-ralph -R 'skygate-ephemeris-(api-model|engine-interface-migration)-tests' --output-on-failure` passed.
- Full `ctest --test-dir build-ralph --output-on-failure` reported 103/104 tests passing. The only failure was unrelated: `skygate-ui-qml-main-window-tests`, `QmlMainWindowTests::footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()` at `apps/skygate-ui/tests/qml/QmlMainWindowTests.cpp(219)`.
- Added follow-up task HP-052 to `IMPLEMENTATION_PLAN.md` for that unrelated UI regression.
