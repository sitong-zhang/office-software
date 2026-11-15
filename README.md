# Chenyi Office（胡辰逸定制版）

**A Windows 64-bit office suite with a bespoke launcher, a rebuilt editor ribbon, optional updates, and in-app announcements.**

[![License: MPL 2.0](https://img.shields.io/badge/license-MPL%202.0-blue.svg)](LICENSE)
[![Platform: Windows x64](https://img.shields.io/badge/platform-Windows%2010%2F11%20x64-0078D4.svg)](docs/architecture.md)
[![Version](https://img.shields.io/badge/version-0.4.2-1f5c46.svg)](https://github.com/sitong-zhang/office-software/releases/latest)

## Status

**v0.4.2 — Windows preview with a fully configured editor interface.** Writer, Calc, Impress and Draw now launch directly into the tabbed notebookbar with the full set of layout variants available from View → User Interface, so the interface is never locked. The About dialog is reachable, the Start Center is fully localized, and a coordinated icon theme is applied. Updates are optional: the current build stays usable until you choose to install, and the downloaded installer is accepted only when it carries the pinned Chenyi Office Authenticode signature.

## Download

- **Installer:** [ChenyiOffice-0.4.2-x64.exe](https://github.com/sitong-zhang/office-software/releases/latest)
- **Platform:** Windows 10 or Windows 11, 64-bit
- **Notes:** the install password is requested only for the first installation. It is distributed separately from this repository. Updates never ask for it. The launcher checks for a newer release on every start and offers it without interrupting your work.

## What the launcher does

1. Shows the branded splash animation.
2. Reads `latest.json` from the latest GitHub Release. If a newer version exists, a non-blocking update prompt offers an immediate update and a reminder; the current build stays usable. A chosen update is downloaded, verified against its SHA-256 and the pinned publisher signature, installed silently with `/S`, and then relaunches.
3. Reads `announcements.json` from the repository and shows unseen notices once, dismissed with **继续**.
4. Prepares a dedicated engine profile with the Chenyi Office interface settings and starts the editor.

## Interface layer

The installer writes a signed-in launcher, a brand profile, and the generated editor layout. On first start, the launcher creates `%LOCALAPPDATA%\ChenyiOffice\lo-profile` and launches the engine with `-env:UserInstallation`, so the office UI uses:

- **Tabbed ribbon** in Writer, Calc, Impress, and Draw, reordered into a grouped sequence.
- **编辑** group on the Home tab of all four applications (Find & Replace, Select All).
- **Sifr** icon theme for a quieter, modern look.
- **Chenyi Office** as the product name and vendor.
- Custom splash screens and Start Center / About marks written into the engine at install time.
- No first-run tip, What's New, feedback, or getting-involved entries.
- A clean first-run experience behind the Chenyi Office splash.

When the brand version changes, the launcher merges only missing product defaults into the existing profile and keeps the previous file as `registrymodifications.xcu.bak`. User toolbar, icon, and option changes are preserved across upgrades. See [docs/ui.md](docs/ui.md).

If a compatible engine is already installed, the installer reuses it, leaves its artwork and layout files untouched, and records that fact so uninstalling Chenyi Office does not remove the user's own installation. If that engine is later removed or replaced, the launcher offers a download-page shortcut to reinstall the kernel.

## Product direction

- **Interface rebuild** — one visual language across the launcher, Start Center, editor ribbon, dialogs, and settings. See [docs/ui.md](docs/ui.md).
- **Optional updates** — a new version is offered, never forced; the current build keeps working until you choose to update. See [docs/updater.md](docs/updater.md).
- **In-app announcements** — delivered at launch, independent of releases. See [docs/announcements.md](docs/announcements.md).
- **Windows 64-bit only**; macOS and Linux builds are not planned.

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
│   ├── engine-ui/        # Generated editor ribbon, menu and Start Center layouts
│   └── launcher/         # Splash, optional update prompt, announcements, engine start
├── installer/            # Password-gated welcome page and Windows installer
├── uninstaller/          # Uninstall entry
├── docs/                 # Project site, architecture, UI, updater, announcements, roadmap
├── scripts/              # Brand asset generation, engine UI generation, Linux cross-build
├── announcements.json    # In-app announcement feed
├── CHANGELOG.md          # User-visible changes per release
└── README.md
```

## Building

The Windows installer is cross-built on Linux with MinGW-w64; no Windows machine is required.

Requirements: `mingw-w64`, `msitools`, `python3` with Pillow, `curl`, `osslsigncode`, `openssl`.

```bash
SIGN_PFX_PASS=... ./scripts/build-windows.sh
```

The script generates brand assets and the editor layout, compiles and signs the launcher and uninstaller, verifies the pinned branded engine MSI against its SHA-256, compiles the installer, appends the engine payload, and signs the final installer. For release builds set `REQUIRE_SIGN=1` so missing signing material is a hard failure. Output: `dist/ChenyiOffice-0.4.2-x64.exe` plus its SHA-256 and the public signing certificate.

See [docs/build.md](docs/build.md) for the pipeline, [docs/signing.md](docs/signing.md) for the code-signing details, and [docs/architecture.md](docs/architecture.md) for how the launcher and engine fit together.

## Security

The install password is a distribution gate, not a cryptographic protection: the payload can be extracted by anyone who obtains the installer. Do not reuse it for anything sensitive. Updates are accepted only from an installer signed by the pinned Chenyi Office certificate, even though the certificate is self-signed; Windows shows "Unknown publisher" until the certificate is trusted. See [docs/signing.md](docs/signing.md) and [SECURITY.md](SECURITY.md).

## Contributing

The shell uses a consistent layout and design-token system; [docs/ui-references.md](docs/ui-references.md) records the sources and licenses of the bundled interface assets.

Read [CONTRIBUTING.md](CONTRIBUTING.md) and the [Code of Conduct](CODE_OF_CONDUCT.md) first. Interface work, launcher fixes, documentation, and translation are all welcome.

## License

Source code in this repository is licensed under the [Mozilla Public License 2.0](LICENSE). The bundled engine includes components under the same license, and the required license texts ship with the installation. Product icons come from [Google Material Design Icons](https://github.com/google/material-design-icons) under the Apache License 2.0; the pinned source, commit, and license are in `app/branding/third-party/material-design-icons/`. See [NOTICE](NOTICE) for third-party notices.
