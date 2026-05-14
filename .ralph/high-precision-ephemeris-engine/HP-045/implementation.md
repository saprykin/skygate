## Task
- ID: HP-045
- Title: Surface degraded-result warnings and provenance in UI

## Status
READY

## Acceptance criteria claimed
- [x] Selection inspector payload carries ephemeris status, warning text,
  provenance, validity range, uncertainty, and correction summary fields.
- [x] Object inspector displays degraded/out-of-range ephemeris status with
  warning tooltip text.
- [x] High-precision provenance, range, uncertainty, and applied correction
  details are only shown when the request is using high precision.
- [x] UI and payload tests cover degraded warning display and propagated
  metadata.
- [x] Existing tests pass in `build-ralph`.

## Files changed
- `apps/skygate-ui/qml/sky/SkyObjectInspector.qml`
- `apps/skygate-ui/src/scene/SkySceneOverlayAdapter.cpp`
- `apps/skygate-ui/src/scene/SkySceneOverlayData.hpp`
- `apps/skygate-ui/src/selection/SkyObjectInspectorBuilder.cpp`
- `apps/skygate-ui/src/selection/SkyObjectInspectorFormatters.cpp`
- `apps/skygate-ui/src/selection/SkyObjectInspectorFormatters.hpp`
- `apps/skygate-ui/tests/qml/QmlSkyOverlayLayerPayloadRenderingTests.cpp`
- `apps/skygate-ui/tests/scene/SkySceneOverlayAdapterTests.cpp`
- `apps/skygate-ui/tests/selection/SkySelectionOverlayBuilderTests.cpp`
- `apps/skygate-ui/tests/support/SkyOverlayTestSupport.hpp`

## Important notes
- Verification: `cmake --build build-ralph -j 2`.
- Verification: `ctest --test-dir build-ralph --output-on-failure`.
- Full CTest passed 125/125; existing CALCEPH kernel provider and solar-system
  calculator tests were skipped by the current build configuration.
