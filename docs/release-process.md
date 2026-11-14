# Release process

This document defines how builds are versioned, published, and announced. It is the authoritative checklist for maintainers.

## Versioning

Versions follow [Semantic Versioning](https://semver.org/):

- `MAJOR` — incompatible changes to file formats, extensions, or configuration
- `MINOR` — new features and user-visible improvements
- `PATCH` — bug fixes and security updates

Pre-releases use suffixes: `1.0.0-beta.1`, `1.0.0-rc.1`. Tags are prefixed with `v` (`v1.0.0`).

## Checklist

1. **Freeze** — merge only release-blocking fixes into the release branch.
2. **Changelog** — move entries from `Unreleased` to the new version in [CHANGELOG.md](../CHANGELOG.md), with the release date.
3. **Build** — produce artifacts on all supported platforms from the pinned toolchain.
4. **Verify** — run the document round-trip suite, installer smoke tests, and the branding checklist in [branding.md](branding.md).
5. **Sign** — sign binaries and checksums; record the signing identity.
6. **Tag** — create an annotated tag and push it:

   ```bash
   git tag -a v1.0.0 -m "Office Software 1.0.0"
   git push origin v1.0.0
   ```

7. **Publish** — create a GitHub Release from the tag:
   - title: `Office Software 1.0.0`
   - notes: the changelog section, plus upgrade notes and known issues
   - assets: installers, checksums, signatures, and NOTICE
   - mark pre-releases accordingly
8. **Announce** — open a thread in [Discussions → Announcements](https://github.com/sitong-zhang/office-software/discussions/categories/announcements) linking the release. Keep it short: what changed, who it affects, and where to download.
9. **Site** — update the latest-release block on the [project site](https://sitong-zhang.github.io/office-software/).
10. **Retrospective** — note any process issues in the next maintainer sync.

## Release notes template

```markdown
## Highlights

- ...

## Changes

- ...

## Upgrade notes

- ...

## Known issues

- ...
```

## Update channels

- **Stable** — latest non-prerelease GitHub Release.
- **Beta** — pre-release builds for testing.

Update manifests, when introduced, are published as release assets and must be signed with the same key as the binaries.
