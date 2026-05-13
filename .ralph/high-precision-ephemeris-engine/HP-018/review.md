## Verdict

NEEDS_FIX

## Task reviewed

- ID: HP-018
- Title: Define ephemeris data manifest schema and parser
- Source: Spec: Data Management
- Base ref: fafbc2767f6a6b194d66c631e82b4fe7bf0599a6
- Head ref: 37af979021d16155708b48372fa4cbd0afdf501c

## Summary

The implementation adds a public ephemeris data manifest model, a JSON parser/validator, CMake wiring, and focused Qt tests for the expected modern and optional DE441 profiles. The scope is appropriate and the build/test suite passes, but the parser still accepts ambiguous or malformed manifest structures that should be rejected at this schema boundary.

## Checks performed

- [x] Read active task
- [x] Read implementation handoff
- [x] Read relevant specs
- [x] Inspected git diff
- [x] Inspected relevant tests
- [x] Ran relevant tests, if safe/applicable

## Findings

### Finding 1: Duplicate manifest IDs are accepted

Severity: MAJOR
File: `libs/skygate-ephemeris/src/engine/highprecision/EphemerisDataManifest.cpp`
Lines/functions: `validateProfileAssetReferences`, `EphemerisDataManifest::profile`, `EphemerisDataManifest::asset`

Problem:
The parser validates references between profiles and assets, but it never rejects duplicate profile IDs, duplicate asset IDs, or repeated asset IDs inside a profile. The public lookup helpers return the first matching entry, so a manifest with two assets using the same ID can parse as valid while one asset is hidden from lookup.

Why it matters:
The manifest is intended to drive future asset activation, checksum verification, and profile selection. Duplicate IDs make that schema ambiguous and can cause later code to verify or activate a different asset than the one implied by a profile entry.

Recommended fix:
During parsing or final validation, reject duplicate `profiles[].id`, duplicate `assets[].id`, and duplicate entries in each `profiles[].assetIds` array. Add tests that malformed duplicate-ID manifests fail with diagnostics.

### Finding 2: Boolean fields are silently coerced

Severity: MAJOR
File: `libs/skygate-ephemeris/src/engine/highprecision/EphemerisDataManifest.cpp`
Lines/functions: `parseProfile`, `parseAsset`

Problem:
`bundled`, `longRange`, and `optional` are read with `QJsonValue::toBool(false)`. Missing values and wrong JSON types such as strings or numbers are accepted as `false` without a diagnostic.

Why it matters:
HP-018 requires the manifest profiles to distinguish bundled modern data from optional DE441 long-range data, and the parser is supposed to validate the JSON manifest structure. Silently coercing invalid boolean fields can turn a malformed manifest into a valid one while changing profile semantics.

Recommended fix:
Validate these fields explicitly. Require `bundled` and `longRange` to be JSON booleans on profiles, and if `optional` is present require it to be a JSON boolean. Add negative tests for string/number/null boolean fields.

### Finding 3: Large compression sizes can overflow during parsing

Severity: MAJOR
File: `libs/skygate-ephemeris/src/engine/highprecision/EphemerisDataManifest.cpp`
Lines/functions: `readUInt64`

Problem:
`readUInt64()` validates JSON numeric values as finite non-negative integral `double`s, then casts directly to `std::uint64_t` without checking that the value is within `uint64_t` range or exactly representable. Extremely large JSON numbers can therefore pass validation and produce an implementation-dependent size value.

Why it matters:
Compression sizes are part of the manifest validation boundary and will later be used for archive handling and integrity checks. Accepting lossy or overflowing size metadata weakens validation and can lead to incorrect cache/download behavior.

Recommended fix:
Reject numbers greater than `std::numeric_limits<std::uint64_t>::max()` and reject integers that cannot be represented exactly as `std::uint64_t`. Consider parsing size fields from JSON text or strings if exact 64-bit values above JavaScript-safe integer range need to be supported. Add boundary tests for oversized and non-exact size values.

## Test assessment

The new `skygate-ephemeris-data-manifest-tests` target covers valid manifests, malformed payloads, missing required fields, checksum and compression metadata, validity ranges, modern profile parsing, optional DE441 profile parsing, and unknown profile/asset references. Missing coverage corresponds to the findings above: duplicate IDs, invalid boolean field types, and compression size boundary handling.

I built `skygate-ephemeris-data-manifest-tests`, ran that target through CTest, and ran the full existing `build-ralph` CTest suite. All 55 tests passed.

## Regression risk

Medium

The change is isolated to a new manifest parser/model and tests, so existing runtime behavior is unlikely to regress. The risk is in accepting malformed manifests that later data-management tasks may treat as trustworthy.

## Out-of-scope observations

The spec says data assets must be zstd-compressed, but HP-019 owns zstd archive handling and first-use activation. I did not treat the current acceptance of `compression.format: "none"` as blocking for HP-018.

## Final recommendation

NEEDS_FIX: fix listed findings, then rerun review.
