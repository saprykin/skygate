## Task
- ID: HP-008E
- Title: Add factory request/result behavior tests

## Status
READY

## Acceptance criteria claimed
- [x] Focused factory behavior tests added for request construction, result helpers, compatibility overloads, fallback policy, strict failure, diagnostics, and invalid requests
- [x] Factory behavior test target registered in the ephemeris CTest flow
- [x] `build-ralph` full build passes
- [x] Relevant ephemeris factory/API tests pass

## Files changed
- `libs/skygate-ephemeris/tests/CMakeLists.txt`
- `libs/skygate-ephemeris/tests/engine/EphemerisEngineFactoryBehaviorTests.cpp`

## Verification
- `clang-format -i libs/skygate-ephemeris/tests/engine/EphemerisEngineFactoryBehaviorTests.cpp`
- `cmake -S . -B build-ralph`
- `cmake --build build-ralph --target skygate-ephemeris-engine-factory-behavior-tests -j2`
- `ctest --test-dir build-ralph --output-on-failure -R 'skygate-ephemeris-(api-model|engine-factory-(behavior|selection))-tests'`
- `cmake --build build-ralph -j2`
- `ctest --test-dir build-ralph --output-on-failure`

## Important notes
- Full CTest still fails only on the known unrelated HP-052 issue:
  `skygate-ui-qml-main-window-tests`,
  `QmlMainWindowTests::footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()`
  at `apps/skygate-ui/tests/qml/QmlMainWindowTests.cpp(219)`.
  The run reported 105/106 tests passing.
