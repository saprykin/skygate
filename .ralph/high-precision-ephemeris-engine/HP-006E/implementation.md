## Task
- ID: HP-006E
- Title: Update fake/test engines and interface migration tests

## Status
READY

## Acceptance criteria claimed
- [x] Fake/test engines across ephemeris and UI tests implement request-based interface overloads
- [x] Tests cover metadata defaults for test engines
- [x] Tests cover request-based snapshot compute and request body lookup
- [x] Tests cover `core::SkyContext` compatibility applying engine default options
- [x] Existing simple-engine and affected UI test targets build and pass

## Files changed
- `apps/skygate-ui/tests/diagnostics/PerformanceGuardTests.cpp`
- `apps/skygate-ui/tests/scene/SkySceneFramePipelineTests.cpp`
- `apps/skygate-ui/tests/selection/SkyObjectTrailBuilderTests.cpp`
- `libs/skygate-ephemeris/tests/CMakeLists.txt`
- `libs/skygate-ephemeris/tests/engine/BodyTrailCalculatorTests.cpp`
- `libs/skygate-ephemeris/tests/engine/EphemerisEngineFallbackTests.cpp`
- `libs/skygate-ephemeris/tests/engine/EphemerisEngineInterfaceMigrationTests.cpp`
- `libs/skygate-ephemeris/tests/events/ObservationEventCalculatorTests.cpp`

## Important notes
- `cmake --build build-ralph --target skygate-ephemeris-engine-interface-migration-tests skygate-ephemeris-body-trail-calculator-tests skygate-ephemeris-engine-fallback-tests skygate-ephemeris-observation-event-calculator-tests skygate-ui-sky-scene-frame-pipeline-tests skygate-ui-sky-object-trail-builder-tests skygate-ui-performance-guard-tests`: PASS
- `ctest --test-dir build-ralph -R 'skygate-ephemeris-engine-interface-migration-tests|skygate-ephemeris-body-trail-calculator-tests|skygate-ephemeris-engine-fallback-tests|skygate-ephemeris-observation-event-calculator-tests|skygate-ui-sky-scene-frame-pipeline-tests|skygate-ui-sky-object-trail-builder-tests|skygate-ui-performance-guard-tests' --output-on-failure`: PASS
- `cmake --build build-ralph`: PASS
- `ctest --test-dir build-ralph --output-on-failure`: FAIL, with only the existing HP-051 `skygate-ui-qml-main-window-tests` footer popup toolbar failure.
