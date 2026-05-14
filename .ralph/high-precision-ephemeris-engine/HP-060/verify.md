# Verdict

PASS

# Task verified

- ID: HP-060
- Title: Wire production ephemeris data acquisition
- Source: IMPLEMENTATION_PLAN.md
- Base ref: 6a64435b44579bb306a2385b82683f279cec9061
- Head ref: 67ad8673df2b5e2a87529e0900600f26a9569c6f

# Summary

HP-060 wires packaged startup manifest loading, controller ephemeris data
inputs, source URL staging, verification, activation, and long-range profile
selection. The review found four blockers; the fix pass addressed packaged
resource ownership, production checksum metadata, DE441 profile ownership, and
the missing bundled modern-data claim. I verified those closures in code and
tests, ran the focused HP-060 tests, and ran the full suite. The task is ready
for final acceptance.

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

- Finding: Packaged startup does not load the manifest
  - Original severity: BLOCKER
  - Closure status: Resolved
  - Notes: The ephemeris qrc is now linked into the `skygate-ui`
    executable, startup searches `:/ephemeris`, and the packaged smoke test
    requires the manifest-loaded log while rejecting the missing-manifest
    warning.

- Finding: Production manifest checksums cannot verify
  - Original severity: BLOCKER
  - Closure status: Resolved
  - Notes: Placeholder checksums were replaced with SHA-256 values and
    positive uncompressed sizes. I independently checked the small remote
    assets and DE440s against the committed hashes; DE441 was not downloaded
    because it is multi-gigabyte, but its committed size matches the remote
    `Content-Length`.

- Finding: DE441 profile references assets from another profile
  - Original severity: BLOCKER
  - Closure status: Resolved
  - Notes: `de441-long-range` now references profile-owned kernel,
    leap-second, EOP, and Delta T assets. Tests cover full long-range staged
    activation and active snapshot selection.

- Finding: Bundled modern data is declared but not packaged
  - Original severity: BLOCKER
  - Closure status: Resolved
  - Notes: The production `modern` profile is no longer marked bundled, and
    bundled fallback lookup ignores unbundled profiles. Tests verify the
    production manifest does not expose missing fallback kernels.

# Findings

No findings.

# Test assessment

The task has focused coverage for production manifest metadata, full
long-range activation, source URL staging, packaged startup manifest loading,
controller settings, and acceptance behavior.

Tests run:

- `cmake --build build-ralph --target
  skygate-ui-sky-ephemeris-data-manager-tests skygate-ui`: PASS
- `ctest --test-dir build-ralph --output-on-failure -R
  'skygate-ui-sky-ephemeris-data-manager-tests|
  skygate-ui-packaged-app-smoke|
  skygate-ui-acceptance-matrix-tests|
  skygate-ui-context-controller-ephemeris-settings-tests'`: PASS
- `ctest --test-dir build-ralph --output-on-failure`: PASS
  (126 passed, 1 skipped:
  `skygate-ephemeris-solar-system-state-calculator-tests`)

Additional remote metadata checks:

- IANA leap-second table SHA-256 matched the manifest.
- USNO Delta T data SHA-256 matched the manifest.
- IERS EOP data SHA-256 matched the manifest.
- JPL DE440s SHA-256 matched the manifest.
- JPL DE441 `Content-Length` matched the manifest size; the full SHA-256 was
  not recomputed locally because the file is approximately 3.3 GB.

# Regression risk

Low

The changed behavior is covered by focused manager tests, app acceptance tests,
the packaged smoke test, and the full CTest suite. The remaining risk is mostly
release-packaging-specific validation for actual external resource layouts on
each platform.

# Out-of-scope observations

- The packaged smoke test can be influenced by an inherited
  `SKYGATE_EPHEMERIS_DATA_ROOT` that points to a valid filesystem manifest,
  although the current focused run passed in the normal local environment.
- When no production profile is bundled, status text still says
  `Bundled fallback`; this does not expose missing files but may be refined in
  a separate UI wording task.

# Final recommendation

PASS: ready for final acceptance or merge.
