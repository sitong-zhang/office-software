# Chenyi Office（胡辰逸定制版）

**A Windows 64-bit office suite built on LibreOffice technology, rebuilt around a redesigned interface and a custom startup experience.**

[![License: MPL 2.0](https://img.shields.io/badge/license-MPL%202.0-blue.svg)](LICENSE)
[![Platform: Windows x64](https://img.shields.io/badge/platform-Windows%2010%2F11%20x64-0078D4.svg)](docs/architecture.md)
[![Status: foundation](https://img.shields.io/badge/status-foundation-orange.svg)](docs/roadmap.md)

> **Project status: foundation stage.** Brand identity, UI direction, update infrastructure, and the roadmap are in place. There is no installable build yet.

## Overview

Chenyi Office（胡辰逸定制版） is a customized edition of an office suite built on the LibreOffice engine. The product direction is **interface first**: a calmer, more consistent workspace, simplified menus and toolbars, and a startup experience designed for this edition — rather than shipping the upstream interface unchanged.

The suite targets everyday document work — writing, spreadsheets, presentations, and PDF — on Windows 10 and Windows 11, 64-bit.

## Planned product direction

- **Interface rebuild** — one visual language across the Start Center, editor, dialogs, and settings; less chrome, clearer defaults. See [docs/ui.md](docs/ui.md).
- **Custom splash animation** — a short branded startup animation that doubles as the update check screen.
- **Mandatory updates** — on launch the app reads a release manifest from this repository. When a new version is published, the update screen blocks the app until the new build is installed; it cannot be skipped. See [docs/updater.md](docs/updater.md).
- **Windows 64-bit first** — Windows 10/11 x64 is the only supported target until version 1.0. macOS and Linux are deferred.

## Project links

| Resource | Link |
| --- | --- |
| Project site | https://sitong-zhang.github.io/office-software/ |
| Downloads | [Releases](https://github.com/sitong-zhang/office-software/releases) |
| Update manifest | https://github.com/sitong-zhang/office-software/releases/latest/download/latest.json |
| Announcements | [Discussions → Announcements](https://github.com/sitong-zhang/office-software/discussions/categories/announcements) |
| Bug reports | [Issues](https://github.com/sitong-zhang/office-software/issues) |
| Questions and ideas | [Discussions](https://github.com/sitong-zhang/office-software/discussions) |

## Roadmap

The staged plan — foundation, Windows client and identity, interface rebuild, preview, beta, and 1.0 — lives in [docs/roadmap.md](docs/roadmap.md). Milestones are tracked in [GitHub Milestones](https://github.com/sitong-zhang/office-software/milestones).

## Repository layout

```text
.
├── .github/            # Issue forms, pull request template, update manifest workflow
├── docs/               # Project site, architecture, UI, updater, branding, roadmap, release process
├── CHANGELOG.md        # User-visible changes per release
├── CONTRIBUTING.md     # How to contribute
├── LICENSE             # Mozilla Public License 2.0
├── NOTICE              # Upstream attribution and trademark notice
└── README.md
```

The upstream source tree will be integrated once the build plan is settled; see [docs/architecture.md](docs/architecture.md) for the intended patch-based strategy. Until then this repository holds project-level material only.

## Building

There is no build yet. The first build target is Windows 10/11 x64, with MSI and per-user installer variants. Build instructions will be published together with the first source integration. Watch the repository or the [announcements](https://github.com/sitong-zhang/office-software/discussions/categories/announcements) for updates.

## Contributing

Contributions are welcome once the development workflow is in place. In the meantime, you can open issues, join discussions, and review the planning documents. Please read [CONTRIBUTING.md](CONTRIBUTING.md) and the [Code of Conduct](CODE_OF_CONDUCT.md) first.

## Security

Please report vulnerabilities privately — see [SECURITY.md](SECURITY.md). Do not open public issues for security problems.

## License and trademarks

Source code in this repository is licensed under the [Mozilla Public License 2.0](LICENSE), the same license as the upstream project it is based on. Upstream attribution and trademark terms are described in [NOTICE](NOTICE) and [docs/branding.md](docs/branding.md).

LibreOffice is a trademark of The Document Foundation. This project is independent and is not affiliated with, endorsed by, or sponsored by The Document Foundation.
