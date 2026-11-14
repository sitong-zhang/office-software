# Release process

This document defines how builds are versioned, published, and announced. It is the authoritative checklist for maintainers.

## Versioning

Versions follow [Semantic Versioning](https://semver.org/):

- `MAJOR` — incompatible changes to file formats, extensions, or configuration
- `MINOR` — new features and user-visible improvements
- `PATCH` — bug fixes and security updates

Pre-releases use suffixes: `1.0.0-beta.1`, `1.0.0-rc.1`. Tags are prefixed with `v` (`v1.0.0`). The update manifest is only generated for published, non-prerelease releases, because clients read `releases/latest`.

## Checklist

1. **Freeze** — merge only release-blocking fixes into the release branch.
2. **Changelog** — move entries from `Unreleased` to the new version in [CHANGELOG.md](../CHANGELOG.md), with the release date.
3. **Build** — produce the Windows 10/11 x64 installer from the pinned toolchain. The filename must include `x64`.
4. **Verify** — run the document round-trip suite, installer smoke tests on Windows 10 and 11, and the branding checklist in [branding.md](branding.md).
5. **Sign** — sign the installer with Authenticode and record the signing identity. Sign the manifest when manifest signing is enabled.
6. **Tag** — create an annotated tag and push it:

   ```bash
   git tag -a v1.0.0 -m "Chenyi Office（胡辰逸定制版） 1.0.0"
   git push origin v1.0.0
   ```

7. **Publish** — create a GitHub Release from the tag:
   - title: `Chenyi Office（胡辰逸定制版） 1.0.0`
   - notes: the changelog section, plus upgrade notes and known issues
   - assets: x64 installer, checksums, signatures, and NOTICE
   - do not mark the release as a pre-release
8. **Manifest** — confirm `.github/workflows/update-manifest.yml` attached `latest.json` with the correct version, hash, size, and installer URL. Clients are blocked from the old build once this asset exists.
9. **Announce** — open a thread in [Discussions → Announcements](https://github.com/sitong-zhang/office-software/discussions/categories/announcements) linking the release. State that the update is mandatory.
10. **Site** — update the latest-release block on the [project site](https://sitong-zhang.github.io/office-software/).
11. **Retrospective** — note any process issues in the next maintainer sync.

## Release notes template

```markdown
## Highlights

- ...

## Changes

- ...

## Upgrade notes

- This update is mandatory: the previous build is blocked after the release goes live.

## Known issues

- ...
```

## Update channels

- **Stable** — the latest published release; the only channel clients use.
- **Beta** — pre-release builds for testing. Beta builds check the same manifest; publishing a pre-release does not trigger a manifest update.

## Rollback

The updater never installs a lower version. To roll back, publish a fixed build with a higher version number and let the mandatory update flow deliver it.
