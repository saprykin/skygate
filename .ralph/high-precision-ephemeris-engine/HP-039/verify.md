# Verdict

PASS

# Task verified

- ID: HP-039
- Title: Add ephemeris user settings snapshot
- Source: Spec: Engine Selection; Settings
- Base ref: 62ff37fa5177d1f8109cad6c806331feb1498e90
- Head ref: fb42fe876b5f625d86edeec0710ce710afc8eec3

# Summary

HP-039 adds persisted ephemeris user settings for engine selection, correction
flags, refraction/atmosphere defaults, preferred data profile, and update
preferences while leaving ephemeris data-cache metadata separate. The review
found that the controller path dropped profile/update fields and failed to
notify scene consumers after reload. The fix preserves the full controller
snapshot and emits `skyContextChanged()` after applying persisted ephemeris
settings. I verified the diff, inspected the relevant code and tests, and ran
the focused and full configured test suites. The task is ready to accept.

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

- Finding: Controller Drops Profile And Update Settings
  - Original severity: MAJOR
  - Closure status: Resolved
  - Notes: `SkyContextController` now stores the full ephemeris user settings
    snapshot, loads all persisted ephemeris user fields, and saves the retained
    profile/update fields while overlaying current live engine option fields.
    Controller round-trip coverage verifies custom profile and update settings
    survive load/save.

- Finding: Ephemeris Reload Does Not Notify Scene Consumers
  - Original severity: MAJOR
  - Closure status: Resolved
  - Notes: `loadSettings()` now emits `skyContextChanged()` after applying
    persisted ephemeris settings and rebuilding the ephemeris engine.
    Controller-level coverage verifies the notification.

# Findings

No findings.

# Test assessment

Existing and new tests cover direct settings-store persistence, snapshot
split/merge behavior, malformed and partial value fallback, controller
load/save preservation of all ephemeris user fields, and the reload
notification expected by scene consumers.

Tests run:

- `cmake --build build-ralph --target
  skygate-ui-context-controller-ephemeris-settings-tests
  skygate-ui-settings-store-tests
  skygate-ui-sky-settings-codecs-tests -j2`: PASS
- `ctest --test-dir build-ralph --output-on-failure -R
  'ephemeris-settings|settings-store|sky-settings-codecs'`: PASS, 3/3
- `ctest --test-dir build-ralph --output-on-failure`: PASS, 124/124
  configured tests passed; the two configured CALCEPH-dependent tests were
  skipped.

# Regression risk

Low

The changes are localized to settings snapshot codecs, controller settings
load/save wiring, and focused tests. The full configured test suite passes.

# Out-of-scope observations

- The prior review noted that atmosphere setting codecs accept finite but
  physically out-of-range values and rely on later validation. That remains a
  possible follow-up, but it does not block HP-039 because malformed values and
  persistence behavior are covered.

# Final recommendation

PASS: ready for final acceptance or merge.
