# Architecture

This document records how the project is organized and how the upstream LibreOffice codebase and update pipeline will be integrated. It describes the intended design; the source tree is not part of this repository yet.

## Scope

Chenyi Office（胡辰逸定制版） targets **Windows 10 and Windows 11, 64-bit only** until version 1.0. macOS, Linux, mobile, and portable builds are out of scope.

## Repository layout

This repository is the product repository. It holds:

- product documentation, branding assets, and release notes;
- the project site served by GitHub Pages;
- build and packaging configuration;
- patches and downstream-only modules applied to upstream;
- the update manifest workflow.

The upstream source tree is large (several gigabytes with history). Cloning or vendoring it into this repository would make the product repository impractical to use and would obscure downstream changes. Instead, the build works from a pinned upstream revision plus the patches tracked here.

## Upstream integration strategy

Two options are under evaluation:

1. **Patch series (preferred).** A script fetches the pinned upstream tag, applies the patches in `patches/` in order, and builds the result. Downstream changes stay reviewable, rebasing onto new upstream releases is explicit, and the repository stays small.
2. **Vendored tree.** A subtree or full copy of upstream under `src/`. Simpler to build, but makes every upstream update a large merge and inflates the repository.

The decision, including the pinned upstream version and the patch naming convention, will be recorded in this document before source integration begins.

## Build and packaging

| Target | Artifact | Notes |
| --- | --- | --- |
| Windows 10/11 x64 | MSI installer | Per-user and per-machine variants |
| Windows 10/11 x64 | EXE bootstrapper | Wraps the MSI and simplifies elevation |

Builds are produced from a pinned toolchain, with checksums and Authenticode signatures published alongside each release. The updater expects installer assets to include `x64` in the filename so the manifest workflow can identify them.

## Update pipeline

The update path is a first-class part of the architecture:

1. A release is published with an x64 installer attached.
2. `.github/workflows/update-manifest.yml` computes the SHA-256 and size, writes `latest.json`, and attaches it to the same release.
3. The application fetches `releases/latest/download/latest.json` at startup.
4. If the published version is newer, a blocking update screen requires installation before use.

The full protocol, manifest schema, failure handling, and security requirements are defined in [updater.md](updater.md).

## Continuous integration

Planned jobs:

- Windows x64 build verification for every pull request that touches build configuration or source patches;
- document round-trip smoke tests for a fixed corpus of files;
- license and branding checks that fail if upstream notices are removed or upstream trademarks appear in product surfaces;
- installer smoke tests on Windows 10 and 11 images;
- release automation that drafts a GitHub Release from `CHANGELOG.md` and publishes the update manifest.

## Update and announcement channels

- **Releases** — signed installers, checksums, and release notes.
- **Update manifest** — `latest.json`, attached to each release; the only source the client trusts.
- **Discussions → Announcements** — product news and maintenance notices.
- **Project site** — a stable, linkable summary of the latest release and the roadmap.

Clients do not query the GitHub API directly. They read the manifest asset, which is generated at release time and can be signed independently.
