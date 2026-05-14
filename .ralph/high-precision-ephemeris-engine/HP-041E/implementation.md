## Task
- ID: HP-041E
- Title: Apply selected engine and request to trails

## Status
READY

## Acceptance criteria claimed
- [x] Trail sampling routes through request-based ephemeris APIs when a
      selected-engine request is available.
- [x] Trail samples preserve selected correction options across sampled epochs.
- [x] Trail samples update both `SkyContext::utcTime` and the astronomical
      request epoch for each sample.
- [x] Simple-engine compatibility path remains available.
- [x] Tests added for request-aware trail sampling and trail rendering.
- [x] Existing tests pass.

## Files changed
- `libs/skygate-ephemeris/include/skygate/ephemeris/BodyTrailCalculator.hpp`
- `libs/skygate-ephemeris/src/BodyTrailCalculator.cpp`
- `libs/skygate-ephemeris/tests/engine/BodyTrailCalculatorTests.cpp`
- `apps/skygate-ui/src/selection/SkyObjectTrailBuilder.hpp`
- `apps/skygate-ui/src/selection/SkyObjectTrailBuilder.cpp`
- `apps/skygate-ui/src/scene/SkySceneComposition.cpp`
- `apps/skygate-ui/tests/selection/SkyObjectTrailBuilderTests.cpp`

## Verification
- `cmake --build build-ralph -j2`
- Targeted CTest:
  ```
  ctest --test-dir build-ralph -R \
      'skygate-ephemeris-body-trail-calculator-tests' \
      --output-on-failure
  ```
- Targeted CTest:
  ```
  ctest --test-dir build-ralph -R \
      'skygate-ui-sky-object-trail-builder-tests' \
      --output-on-failure
  ```
- `ctest --test-dir build-ralph --output-on-failure`

## Important notes
- Full CTest passed: 124/124 tests passed. CALCEPH-dependent tests 33 and 34
  were skipped by the existing build configuration.
