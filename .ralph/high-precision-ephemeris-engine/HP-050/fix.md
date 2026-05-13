## Task fixed
  - ID: HP-050
  - Title: Rename catalog public `None` enumerators to avoid macro collisions
  - Source: IMPLEMENTATION_PLAN.md, HP-050

## Review input
  - Review verdict: NEEDS_FIX
  - Review report: `.ralph/high-precision-ephemeris-engine/HP-050/review.md`
  - Implementation handoff: `.ralph/high-precision-ephemeris-engine/HP-050/implementation.md`

## Summary
  Restored source compatibility for existing macro-clean callers by adding guarded deprecated aliases for the old public `None` enum spellings. The aliases are omitted when an X11-style `None` macro is defined, preserving the macro-collision fix.

## Findings addressed
  - Finding title: Old public enum spellings were removed instead of compatibility-aliased
  - Severity: MAJOR
  - Action: Fixed
  - File(s): `libs/skygate-ephemeris/include/skygate/ephemeris/CatalogLoader.hpp`, `libs/skygate-ephemeris/include/skygate/ephemeris/CatalogLoadResult.hpp`, `libs/skygate-ephemeris/tests/CMakeLists.txt`, `libs/skygate-ephemeris/tests/engine/PublicHeaderMacroCompatTests.cpp`, `libs/skygate-ephemeris/tests/engine/PublicHeaderLegacyAliasTests.cpp`
  - What changed: Added `CatalogSelectionMode::None` as a deprecated alias for `CatalogSelectionMode::Disabled` and `CatalogLoadErrorCode::None` as a deprecated alias for `CatalogLoadErrorCode::NoError`, each guarded by `#ifndef None`. Added a separate macro-clean test translation unit that references the legacy aliases and linked it into the public-header compatibility test target.
  - Why this resolves the finding: Existing downstream code can still compile with the old enum spellings when no platform `None` macro is present, while public headers remain safe to include after a platform header or macro definition that defines `None`.

## Tests run
  - `cmake --build build-ralph --target skygate-ephemeris-public-header-macro-compat-tests skygate-ephemeris-catalog-factory-tests skygate-ephemeris-catalog-property-tests`: PASS
  - `ctest --test-dir build-ralph --output-on-failure -R 'skygate-ephemeris-(public-header-macro-compat|catalog-factory|catalog-property)-tests'`: PASS
  - `ctest --test-dir build-ralph --output-on-failure`: FAIL, 102/103 tests passed; `skygate-ui-qml-main-window-tests` failed in `QmlMainWindowTests::footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()` at `apps/skygate-ui/tests/qml/QmlMainWindowTests.cpp(219)`, matching the known unrelated HP-051 issue.

## Files changed
  - `libs/skygate-ephemeris/include/skygate/ephemeris/CatalogLoader.hpp`
  - `libs/skygate-ephemeris/include/skygate/ephemeris/CatalogLoadResult.hpp`
  - `libs/skygate-ephemeris/tests/CMakeLists.txt`
  - `libs/skygate-ephemeris/tests/engine/PublicHeaderMacroCompatTests.cpp`
  - `libs/skygate-ephemeris/tests/engine/PublicHeaderLegacyAliasTests.cpp`
  - `.ralph/high-precision-ephemeris-engine/HP-050/implementation.md`
  - `.ralph/high-precision-ephemeris-engine/HP-050/fix.md`

## Remaining concerns
  The unrelated `skygate-ui-qml-main-window-tests` failure remains and is already tracked by HP-051.

## Final fixer status
  READY_FOR_REVIEW
