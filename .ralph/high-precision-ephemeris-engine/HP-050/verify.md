# Verdict

PASS

# Task verified

- ID: HP-050
- Title: Rename catalog public `None` enumerators to avoid macro collisions
- Source: IMPLEMENTATION_PLAN.md, HP-050
- Base ref: a5aba1886d3d6cb3ea55822a1af176c5e6506995
- Head ref: 827737454fd9e83324461234e8887fe93f726d70

# Summary

HP-050 renamed the public catalog `None` enumerators to
`CatalogSelectionMode::Disabled` and `CatalogLoadErrorCode::NoError`, updated
in-repo callers, and added public-header include-order coverage for an
X11-style `None` macro. The review found that the old public enum spellings had
been removed without preserving source compatibility. The fix pass added
guarded deprecated aliases for macro-clean include contexts and added compile
coverage for those aliases. The task is implemented, the review finding is
resolved, relevant tests pass, and the remaining full-suite failure is the
already-tracked unrelated HP-051 UI test.

# Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read review report
- [x] Read fixer report, if present
- [x] Read relevant specs
- [x] Inspected git history
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

# Review/fix closure

- Finding: Old public enum spellings were removed instead of compatibility-aliased
  - Original severity: MAJOR
  - Closure status: Resolved
  - Notes: `CatalogSelectionMode::None` and `CatalogLoadErrorCode::None` are now deprecated aliases guarded by `#ifndef None`, so macro-clean callers keep source compatibility while X11-style `None` macro include order remains safe. `PublicHeaderLegacyAliasTests.cpp` verifies the aliases in a macro-clean translation unit.

# Findings

No findings.

# Test assessment

Relevant targeted build passed:
`cmake --build build-ralph --target skygate-ephemeris-public-header-macro-compat-tests skygate-ephemeris-catalog-factory-tests skygate-ephemeris-catalog-property-tests`.

Relevant targeted CTest passed:
`ctest --test-dir build-ralph --output-on-failure -R 'skygate-ephemeris-(public-header-macro-compat|catalog-factory|catalog-property)-tests'`
passed 3/3 tests.

The coverage is appropriate for this task. `PublicHeaderMacroCompatTests.cpp`
defines `None` before including the catalog public headers and checks the new
macro-safe spellings. `PublicHeaderLegacyAliasTests.cpp` compiles without a
`None` macro and checks that the old public spellings map to the new values.
The catalog factory and property tests cover updated in-repo catalog behavior.

Full CTest was also run with `ctest --test-dir build-ralph --output-on-failure`.
It passed 102/103 tests. The only failure was
`skygate-ui-qml-main-window-tests`,
`QmlMainWindowTests::footerPopupToolbarToggleClickClosesPopupAndTogglesToolbar()`
at `apps/skygate-ui/tests/qml/QmlMainWindowTests.cpp(219)`, matching the known
unrelated UI regression already tracked as HP-051.

# Regression risk

Low

The public API rename is narrow, in-repo callers were updated, X11-style macro
include order is covered, and macro-clean legacy aliases reduce external source
compatibility risk. The remaining full-suite failure is unrelated to this
catalog public-header task and has a follow-up task.

# Out-of-scope observations

- The persistent `skygate-ui-qml-main-window-tests` failure remains and is
  already tracked by HP-051.
- The verifier prompt references `specs/high-precision-ephemeris-engine.md`,
  but this checkout stores the document at
  `spec/high-precision-ephemeris-engine.md`.

# Final recommendation

PASS: ready for final acceptance or merge.
