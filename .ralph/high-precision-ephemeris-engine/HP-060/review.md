## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-060
- Title: Wire production ephemeris data acquisition
- Source: IMPLEMENTATION_PLAN.md
- Base ref: 6a64435b44579bb306a2385b82683f279cec9061
- Head ref: 577b704a570e6b9ed5dc7c67f2ac8abc68d644e3

## Summary

The implementation adds startup manifest loading, controller update wiring,
source URL staging, and a production manifest. The local staging path and
existing tests work, and the full CTest suite passes. However, the production
path still does not satisfy HP-060: the packaged smoke run does not load the
manifest, production checksums are placeholders, `de441-long-range` cannot
verify, and the bundled modern profile is declared without packaged data.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: Packaged startup does not load the manifest

Severity: BLOCKER
File: `apps/skygate-ui/CMakeLists.txt`
Lines/functions: lines 168-169; `startupEphemerisDataConfiguration`

Problem:
The new ephemeris resource is added to the static `skygate-ui-lib`, but the
packaged app smoke run still logs `No ephemeris data manifest was found.`
`startupEphemerisDataConfiguration()` falls through all candidates, including
`:/ephemeris`, so production startup does not actually load the packaged
manifest. The smoke test only checks for the generic startup log and therefore
passes while this HP-060 requirement is broken.

Why it matters:
HP-060 explicitly requires packaged app startup to load the bundled manifest
and pass it into the controller. Without that, production update controls and
bundled fallback wiring start as manifest-unavailable.

Recommended fix:
Ensure the ephemeris Qt resource is initialized in the executable, for example
by attaching it to `skygate-ui` or calling the generated `Q_INIT_RESOURCE`
before lookup. Then update `PackagedAppSmoke.cmake` to fail unless the startup
log contains `Loaded ephemeris data manifest` and does not contain the missing
manifest warning.

### Finding 2: Production manifest checksums cannot verify

Severity: BLOCKER
File: `apps/skygate-ui/resources/ephemeris/manifest.json`
Lines/functions: lines 49-52, 71-74, 92-95, 113-116, 134-137

Problem:
Every production asset uses `external-release-metadata-required` as the
`sha256` checksum value. `verifyEphemerisStagedUpdateSet()` computes the
payload hash and compares it exactly with `asset.checksum.value`, so downloads
from the listed source URLs will always fail verification after staging.

Why it matters:
The HP-060 acquisition flow is supposed to download, verify, and activate real
assets. Placeholder checksums make both `Update Modern` and `Install DE441`
non-functional in production.

Recommended fix:
Replace the placeholder values with real SHA-256 checksums for the exact bytes
that will be staged, and include size metadata where available. Add a test that
parses the production manifest and verifies at least a local fixture profile
through the same production-style metadata path.

### Finding 3: DE441 profile references assets from another profile

Severity: BLOCKER
File: `apps/skygate-ui/resources/ephemeris/manifest.json`
Lines/functions: lines 33-37, 88, 109, 130

Problem:
`de441-long-range` includes `leap-seconds`, `earth-orientation`, and `delta-t`,
but those asset records have `profileId: "modern"`. The verifier rejects any
profile asset whose `asset->profileId != request.profileId`, so
`Install DE441` will fail as malformed metadata even if the files download.

Why it matters:
One of the claimed acceptance criteria is that `Install DE441` targets the
explicit `de441-long-range` profile. As committed, that profile cannot pass
the existing verification contract.

Recommended fix:
Either make shared support assets valid for multiple profiles in the manifest
model and verifier, or duplicate the support asset records for
`de441-long-range` with matching profile IDs. Add controller/data-manager test
coverage for a full `de441-long-range` profile update.

### Finding 4: Bundled modern data is declared but not packaged

Severity: BLOCKER
File: `apps/skygate-ui/resources/ephemeris.qrc`
Lines/functions: line 3

Problem:
The manifest marks the `modern` profile as bundled, but the qrc contains only
`manifest.json`. There are no packaged `kernels/de440s.bsp`,
`time/leap-seconds.list`, `time/eop.txt`, or `time/delta-t.data` assets, and
the install rules do not add an external `ephemeris` data directory. The
fallback snapshot also requires the kernel file to exist under the bundled
resource root before exposing it.

Why it matters:
HP-060 requires a clean packaged install to use bundled modern
high-precision data offline when release packaging includes the data. The
committed package metadata says that profile is bundled, but the committed
packaging cannot provide the files the runtime expects.

Recommended fix:
Add the intended modern data package inputs and install/resource rules, or mark
the profile unbundled until release packaging supplies those files. Add a
packaged/offline smoke that starts with no cache and proves the active snapshot
can resolve the bundled modern kernel and support text data.

## Test assessment

Existing tests cover local staged source URL copying, staged verification,
activation preservation, QML control binding, and general packaged startup.
They do not cover production manifest loading in the packaged app, production
manifest activation, HTTP(S) acquisition, or the `de441-long-range` profile.

I ran:

- `cmake --build build-ralph`
- `ctest --test-dir build-ralph --output-on-failure -R '<focused
  HP-060 tests>'`
- `ctest --test-dir build-ralph --output-on-failure`

The full suite passed: 126 passed, 1 skipped
(`skygate-ephemeris-solar-system-state-calculator-tests`).

## Regression risk

High

The failures are in startup resource loading and production data acquisition,
which are the main behavior requested by HP-060. Existing tests pass because
they do not assert the production manifest or real profile metadata path.

## Out-of-scope observations

- The update path exposes an in-progress state but no user-facing cancellation
  control for the new ephemeris download flow.
- The controller operation status can outlive the condition that produced it,
  for example after toggling online updates back on.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
