# Chenyi Office（胡辰逸定制版）

**A Windows 64-bit office suite built on LibreOffice technology, with a bespoke launcher, mandatory updates, and in-app announcements.**

[![License: MPL 2.0](https://img.shields.io/badge/license-MPL%202.0-blue.svg)](LICENSE)
[![Platform: Windows x64](https://img.shields.io/badge/platform-Windows%2010%2F11%20x64-0078D4.svg)](docs/architecture.md)
[![Version](https://img.shields.io/badge/version-0.1.0-1f5c46.svg)](https://github.com/sitong-zhang/office-software/releases/latest)

## Status

**v0.1.0 — first Windows preview.** The repository now builds a working Windows installer that bundles the LibreOffice 26.8.3 engine, a custom splash and update gate, an in-app announcement channel, and a branded welcome page protected by an install password. The LibreOffice interface itself is still upstream; the interface rebuild is the next track.

## Download

- **Installer:** [ChenyiOffice-0.1.0-x64.exe](https://github.com/sitong-zhang/office-software/releases/latest)
- **Platform:** Windows 10 or Windows 11, 64-bit
- **Notes:** the installer requires the install password, distributed separately from this repository. The launcher checks for a newer release on every start; when one exists, the update screen cannot be skipped.

## What the launcher does

1. Shows the branded splash animation.
2. Reads `latest.json` from the latest GitHub Release. If a newer version exists, a blocking update screen downloads, verifies (SHA-256), installs, and restarts — with no exit or skip action.
3. Reads `announcements.json` from the repository and shows unseen notices once, dismissed with **继续**.
4. Starts the bundled office engine.

## Product direction

- **Interface rebuild** — one visual language across the Start Center, editor, dialogs, and settings. See [docs/ui.md](docs/ui.md).
- **Mandatory updates** — the only update path; no "remind me later". See [docs/updater.md](docs/updater.md).
- **In-app announcements** — delivered at launch, independent of releases. See [docs/announcements.md](docs/announcements.md).
- **Windows 64-bit only** until version 1.0; macOS and Linux are deferred.

## Project links

| Resource | Link |
| --- | --- |
| Project site | https://sitong-zhang.github.io/office-software/ |
| Downloads | [Releases](https://github.com/sitong-zhang/office-software/releases) |
| Update manifest | https://github.com/sitong-zhang/office-software/releases/latest/download/latest.json |
| Announcement feed | https://raw.githubusercontent.com/sitong-zhang/office-software/main/announcements.json |
| Announcements | [Discussions → Announcements](https://github.com/sitong-zhang/office-software/discussions/categories/announcements) |
| Bug reports | [Issues](https://github.com/sitong-zhang/office-software/issues) |

## Repository layout

```text
.
├── .github/              # Issue forms, update-manifest and build workflows
├── app/
│   ├── branding/         # Logo and icon sources
│   └── launcher/         # Splash, update gate, announcements, engine start
├── installer/            # Password-gated welcome page and Windows installer
├── uninstaller/          # Uninstall entry
├── docs/                 # Project site, architecture, UI, updater, announcements, roadmap
├── scripts/              # Brand asset generation, Linux cross-build
├── announcements.json    # In-app announcement feed
├── CHANGELOG.md          # User-visible changes per release
└── README.md
```

## Building

The Windows installer is cross-built on Linux with MinGW-w64; no Windows machine is required until the LibreOffice interface is patched at source level.

Requirements: `mingw-w64`, `msitools`, `python3` with Pillow, `curl`.

```bash
./scripts/build-windows.sh
```

The script generates brand assets, compiles the launcher and uninstaller, downloads the pinned LibreOffice MSI, compiles the installer, and appends the engine payload. Output: `dist/ChenyiOffice-0.1.0-x64.exe` plus its SHA-256.

See [docs/build.md](docs/build.md) for the pipeline and [docs/architecture.md](docs/architecture.md) for how the launcher and engine fit together.

## Security

The install password is a distribution gate, not a cryptographic protection: the payload can be extracted by anyone who obtains the installer. Do not reuse it for anything sensitive. Report vulnerabilities according to [SECURITY.md](SECURITY.md).

## Contributing

Read [CONTRIBUTING.md](CONTRIBUTING.md) and the [Code of Conduct](CODE_OF_CONDUCT.md) first. Interface work, launcher fixes, documentation, and translation are all welcome.

## License and trademarks

Source code in this repository is licensed under the [Mozilla Public License 2.0](LICENSE). The bundled LibreOffice engine is distributed under the same license. Upstream attribution and trademark terms are in [NOTICE](NOTICE) and [docs/branding.md](docs/branding.md).

LibreOffice is a trademark of The Document Foundation. This project is independent and is not affiliated with, endorsed by, or sponsored by The Document Foundation.
