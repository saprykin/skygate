# Release Checklist

This checklist is the release path for SkyGate maintainers. It assumes releases
are cut from `master` with tags named `vX.Y.Z`.

## Before Tagging

- Decide the release version, for example `1.1.0`.
- Update `SKYGATE_APP_VERSION` in `CMakeLists.txt`.
- Move relevant entries from `CHANGELOG.md` `[Unreleased]` into a new
  `## [X.Y.Z] - YYYY-MM-DD` section.
- Leave a fresh empty `[Unreleased]` section for the next cycle.
- Commit the version and changelog updates.
- Confirm CI is green on `master` for Linux and Windows, and run the manual
  `CI macOS` workflow if a macOS core preflight is desired.
- Run the manual `Package Linux`, `Package macOS`, and `Package Windows`
  workflows from `master` if a preflight package check is desired.
- Download and smoke-test the manual package artifacts:
  - Linux AppImage starts on a compatible Linux machine.
  - macOS DMG opens and `SkyGate.app` starts.
  - Windows MSI installs/starts on Windows.

## Tag And Package

- Create an annotated release tag:

```bash
git tag -a vX.Y.Z -m "SkyGate vX.Y.Z"
git push origin vX.Y.Z
```

- The tag version must match `SKYGATE_APP_VERSION` in `CMakeLists.txt`. Each
  package workflow validates this before building artifacts.
- Wait for the tag-triggered package workflows to complete:
  - `Package Linux`
  - `Package macOS`
  - `Package Windows`
- Confirm all package jobs are green:
  - Linux AppImage
  - macOS DMG
  - Windows Installer
- Confirm package smoke checks passed before artifacts were uploaded.
- Confirm the GitHub Release was created or updated with:
  - `SkyGate-*-Linux-*.AppImage`
  - `SkyGate-*-Darwin-*.dmg`
  - `SkyGate-*-Windows-*.msi`
- Manual package workflow artifacts are retained for 30 days. Tagged release
  assets attached to GitHub Releases are retained permanently unless removed.

## Release Notes

- Use the version section from `CHANGELOG.md` as the release note source.
- Mention supported package formats and architectures.
- Call out known platform trust limitations until signing/notarization is in
  place:
  - macOS builds are not notarized yet.
  - Windows installers are not code-signed yet.

## After Publishing

- Download the release artifacts from GitHub Releases and run a final install
  check on each supported platform.
- If a package is broken, delete or replace the affected release asset and
  document the replacement in the release notes.
- Open follow-up issues for release blockers that were deferred.

## Future Hardening

- Add macOS code signing and notarization.
- Add Windows code signing.
- Expand package runtime checks if a deeper non-GUI `--smoke-test` mode becomes
  useful.
