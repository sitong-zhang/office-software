# Release process

This document defines how builds are versioned, published, and announced. It is the authoritative checklist for maintainers.

## Versioning

Versions follow [Semantic Versioning](https://semver.org/):

- `MAJOR` — incompatible changes to file formats, extensions, or configuration
- `MINOR` — new features and user-visible improvements
- `PATCH` — bug fixes and security updates

Tags use a `v` prefix (`v0.1.5`). Pre-releases use suffixes (`0.2.0-beta.1`). Only a published, non-prerelease release produces an update manifest, because clients read `releases/latest`.

## Checklist

1. **Freeze** — merge only release-blocking fixes.
2. **Changelog** — move entries from `Unreleased` to the new version in [CHANGELOG.md](../CHANGELOG.md) with the release date.
3. **Build** — run `./scripts/build-windows.sh`. Confirm the installer filename includes `x64` (the manifest workflow depends on it).
4. **Verify** — install on clean Windows 10 and Windows 11 x64 images and check:
   - the welcome page rejects a wrong password and accepts the install password;
   - the engine installs and shortcuts launch the product;
   - the splash appears, the app starts, and announcements display once;
   - a newer test release correctly triggers the blocking update screen;
   - the SHA-256 of the installer matches the checksum file.
5. **Sign** — the build signs the launcher, uninstaller, and installer with the configured certificate (self-signed by default; see [signing.md](signing.md)). Verify with `osslsigncode verify` and note the publisher identity in the release.
6. **Tag** — create an annotated tag and push it:

   ```bash
   git tag -a v0.1.5 -m "Chenyi Office（胡辰逸定制版） 0.1.5"
   git push origin v0.1.5
   ```

7. **Publish** — create a GitHub Release from the tag:
   - title: `Chenyi Office（胡辰逸定制版） 0.1.5`
   - notes: highlights, changes, known issues
   - assets: `ChenyiOffice-0.1.5-x64.exe`, its `.sha256`, and `ChenyiOffice-code-signing.cer`
   - do not mark it as a pre-release.
8. **Manifest** — confirm `.github/workflows/update-manifest.yml` attached `latest.json` with the right version, hash, size, and installer URL. From this point the previous version is blocked on its next launch.
9. **Announce** — update `announcements.json` with a new unique id so installed clients show the release notice, and post in [Discussions → Announcements](https://github.com/sitong-zhang/office-software/discussions/categories/announcements).
10. **Site** — update the latest-release block on the [project site](https://sitong-zhang.github.io/office-software/).
11. **Distribute the install password** through the private channel you use; never publish it in the repository.

## Release notes template

```markdown
## Highlights

- ...

## Changes

- ...

## Known issues

- ...

## Install

Download `ChenyiOffice-<version>-x64.exe`, verify the SHA-256, and enter the install password when prompted for the first installation. The install password is provided separately and is never required for updates.
```

## Update channels

- **Stable** — the latest published release; the only channel clients use.
- **Beta** — pre-release builds for testing. Publishing a pre-release does not update `latest.json`.

## Rollback

The updater never installs a lower version. Publish a fixed build with a higher version number and let the mandatory update flow deliver it.
