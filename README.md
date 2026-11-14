# Chenyi Office（胡辰逸定制版）

**A Windows 64-bit office suite built on LibreOffice technology, with a bespoke launcher, mandatory updates, and in-app announcements.**

[![License: MPL 2.0](https://img.shields.io/badge/license-MPL%202.0-blue.svg)](LICENSE)
[![Platform: Windows x64](https://img.shields.io/badge/platform-Windows%2010%2F11%20x64-0078D4.svg)](docs/architecture.md)
[![Version](https://img.shields.io/badge/version-0.1.4-1f5c46.svg)](https://github.com/sitong-zhang/office-software/releases/latest)

## Status

**v0.1.4 — Windows preview with engine-level branding.** The installer bundles the LibreOffice 26.8.3 engine, a custom launcher, and branded engine artwork. Writer, Calc, Impress, and Draw start in the **Tabbed** notebookbar layout with the **Sifr** icon theme; the product name is **Chenyi Office**, the splash and About/Start Center marks are custom, and upstream tips and promotional infobars are disabled. The installer is Authenticode-signed with a self-signed certificate. Deep VCL-level dialog and widget redesign remains the next track.

## Download

- **Installer:** [ChenyiOffice-0.1.4-x64.exe](https://github.com/sitong-zhang/office-software/releases/latest)
- **Platform:** Windows 10 or Windows 11, 64-bit
- **Notes:** the installer requires the install password, distributed separately from this repository. The launcher checks for a newer release on every start; when one exists, the update screen cannot be skipped.

## What the launcher does

1. Shows the branded splash animation.
2. Reads `latest.json` from the latest GitHub Release. If a newer version exists, a blocking update screen downloads, verifies (SHA-256), installs, and restarts — with no exit or skip action.
3. Reads `announcements.json` from the repository and shows unseen notices once, dismissed with **继续**.
4. Prepares a dedicated LibreOffice profile with the Chenyi Office interface settings and starts the engine.

## Interface layer

The installer writes a signed-in launcher and a brand profile. On first start, the launcher creates `%LOCALAPPDATA%\ChenyiOffice\lo-profile` and launches the engine with `-env:UserInstallation`, so the office UI uses:

- **Tabbed notebookbar** in Writer, Calc, Impress, and Draw.
- **Sifr** icon theme for a quieter, modern look.
- **Chenyi Office** as the product name and vendor.
- Custom splash screens and Start Center / About marks written into the engine at install time.
- No first-run tip, What's New, Get Involved, or Donate infobars.
- A clean first-run experience behind the Chenyi Office splash.

The profile is applied once and then left alone, so user customization is preserved across updates. See [docs/ui.md](docs/ui.md).

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

The script generates brand assets, compiles and signs the launcher and uninstaller, downloads the pinned LibreOffice MSI, compiles the installer, appends the engine payload, and signs the final installer. Output: `dist/ChenyiOffice-0.1.4-x64.exe` plus its SHA-256 and the public signing certificate.

See [docs/build.md](docs/build.md) for the pipeline, [docs/signing.md](docs/signing.md) for the code-signing details, and [docs/architecture.md](docs/architecture.md) for how the launcher and engine fit together.

## Security

The install password is a distribution gate, not a cryptographic protection: the payload can be extracted by anyone who obtains the installer. Do not reuse it for anything sensitive. The installer and launcher carry a self-signed Authenticode signature; Windows shows "Unknown publisher" until the certificate is trusted. See [docs/signing.md](docs/signing.md). Report vulnerabilities according to [SECURITY.md](SECURITY.md).

## Contributing

Read [CONTRIBUTING.md](CONTRIBUTING.md) and the [Code of Conduct](CODE_OF_CONDUCT.md) first. Interface work, launcher fixes, documentation, and translation are all welcome.

## License and trademarks

Source code in this repository is licensed under the [Mozilla Public License 2.0](LICENSE). The bundled LibreOffice engine is distributed under the same license. Upstream attribution and trademark terms are in [NOTICE](NOTICE) and [docs/branding.md](docs/branding.md).

LibreOffice is a trademark of The Document Foundation. This project is independent and is not affiliated with, endorsed by, or sponsored by The Document Foundation.
