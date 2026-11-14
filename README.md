# Office Software

**An independent office suite built on LibreOffice technology, shaped into its own product.**

[![License: MPL 2.0](https://img.shields.io/badge/license-MPL%202.0-blue.svg)](LICENSE)
[![Status: foundation](https://img.shields.io/badge/status-foundation-orange.svg)](docs/roadmap.md)

> **Project status: foundation stage.** The repository is being set up — licensing, branding rules, build and release infrastructure, and the product roadmap. There is no installable build yet.

## Overview

Office Software is a long-term project to build a focused, dependable office suite on top of the LibreOffice codebase. Instead of shipping upstream as-is, the project maintains its own product direction, visual identity, release cadence, and user-facing documentation.

The suite targets everyday document work — writing, spreadsheets, presentations, and PDF — with an emphasis on a clean first-run experience, predictable performance, and a curated feature set.

## Project links

| Resource | Link |
| --- | --- |
| Project site | https://sitong-zhang.github.io/office-software/ |
| Downloads | [Releases](https://github.com/sitong-zhang/office-software/releases) |
| Announcements | [Discussions → Announcements](https://github.com/sitong-zhang/office-software/discussions/categories/announcements) |
| Bug reports | [Issues](https://github.com/sitong-zhang/office-software/issues) |
| Questions and ideas | [Discussions](https://github.com/sitong-zhang/office-software/discussions) |

## Roadmap

The staged plan — foundation, rebranding, first product milestone, beta, and stable release — lives in [docs/roadmap.md](docs/roadmap.md). Milestones are tracked in [GitHub Milestones](https://github.com/sitong-zhang/office-software/milestones).

## Repository layout

```text
.
├── .github/            # Issue forms, pull request template
├── docs/               # Project site, architecture, branding, roadmap, release process
├── CHANGELOG.md        # User-visible changes per release
├── CONTRIBUTING.md     # How to contribute
├── LICENSE             # Mozilla Public License 2.0
├── NOTICE              # Upstream attribution and trademark notice
└── README.md
```

The upstream source tree will be integrated once the build and rebranding plan is settled; see [docs/architecture.md](docs/architecture.md) for the intended strategy. Until then this repository holds only project-level material.

## Building

There is no build yet. Build instructions will be published together with the first source integration and will cover Windows, macOS, and Linux. Watch the repository or the [announcements](https://github.com/sitong-zhang/office-software/discussions/categories/announcements) for updates.

## Contributing

Contributions are welcome once the development workflow is in place. In the meantime, you can open issues, join discussions, and review the planning documents. Please read [CONTRIBUTING.md](CONTRIBUTING.md) and the [Code of Conduct](CODE_OF_CONDUCT.md) first.

## Security

Please report vulnerabilities privately — see [SECURITY.md](SECURITY.md). Do not open public issues for security problems.

## License and trademarks

Source code in this repository is licensed under the [Mozilla Public License 2.0](LICENSE), the same license as the upstream project it is based on. Upstream attribution and trademark terms are described in [NOTICE](NOTICE) and [docs/branding.md](docs/branding.md).

LibreOffice is a trademark of The Document Foundation. This project is independent and is not affiliated with, endorsed by, or sponsored by The Document Foundation.
