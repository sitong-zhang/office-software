# Architecture

This document records how the project is organized and how the upstream LibreOffice codebase will be integrated. It describes the intended design; the source tree is not part of this repository yet.

## Repository layout

This repository is the product repository. It holds:

- product documentation, branding assets, and release notes;
- the project site served by GitHub Pages;
- build and packaging configuration;
- patches and downstream-only modules applied to upstream.

The upstream source tree is large (several gigabytes with history). Cloning or vendoring it into this repository would make the product repository impractical to use and would obscure downstream changes. Instead, the build works from a pinned upstream revision plus the patches tracked here.

## Upstream integration strategy

Two options are under evaluation:

1. **Patch series (preferred).** A script fetches the pinned upstream tag, applies the patches in `patches/` in order, and builds the result. Downstream changes stay reviewable, rebasing onto new upstream releases is explicit, and the repository stays small.
2. **Vendored tree.** A subtree or full copy of upstream under `src/`. Simpler to build, but makes every upstream update a large merge and inflates the repository.

The decision, including the pinned upstream version and the patch naming convention, will be recorded in this document before source integration begins.

## Build and packaging

| Platform | Artifact | Notes |
| --- | --- | --- |
| Windows | MSI installer | Per-user and per-machine variants |
| macOS | Disk image (`.dmg`) | Universal binary where feasible |
| Linux | AppImage, `.deb`, Flatpak | Distribution packages follow the distro's upstream rules |

Builds are produced from a pinned toolchain, with checksums and signatures published alongside each release.

## Continuous integration

Planned jobs:

- build verification on all three platforms for every pull request that touches build or source patches;
- document round-trip smoke tests for a fixed corpus of files;
- license and branding checks that fail if upstream notices are removed or upstream trademarks appear in product surfaces;
- release automation that drafts a GitHub Release from `CHANGELOG.md`.

## Update and announcement channels

- **Releases** — signed binaries, checksums, and release notes.
- **Discussions → Announcements** — product news and maintenance notices.
- **Project site** — a stable, linkable summary of the latest release and the roadmap.

No auto-update mechanism is implemented yet. When it is, clients will read a signed manifest published with each release rather than querying the repository API directly.
