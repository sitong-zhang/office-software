# Changelog

All notable user-visible changes to Chenyi Office（胡辰逸定制版） are documented here. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.2] — 2026-11-15

### Added

- Authenticode SHA-256 signing for the launcher, uninstaller, and installer using a self-signed certificate.
- Public signing certificate attached to the release and documented in [docs/signing.md](docs/signing.md).

### Changed

- Install password updated (distributed separately; only its hash is stored).

### Fixed

- The installer locates its engine payload correctly when an Authenticode certificate table is appended after the payload footer.

## [0.1.1] — 2026-11-15

### Fixed

- Announcement publication times with a non-UTC offset were parsed incorrectly, which could hide new notices.
- The Linux CI build failed with a broken pipe while reading the MSI product code.
- Added launcher logging under `%LOCALAPPDATA%\ChenyiOffice\launcher.log` for update and announcement failures.

## [0.1.0] — 2026-11-15

First Windows preview.

### Added

- Windows 10/11 x64 installer built on Linux with MinGW-w64, bundling LibreOffice 26.8.3.
- Branded welcome page with a background image, logo, product introduction, install-password gate, and a **下一步确认** action.
- Launcher with a custom splash animation, update check, blocking update screen (no exit or skip), in-app announcements, and engine start.
- Brand identity: logo, application icon, installer artwork.
- In-app announcement feed (`announcements.json`) with validation workflow.
- Uninstaller and shell integration (shortcuts, registry entries).

### Changed

- Product named **Chenyi Office（胡辰逸定制版）**.
- Update screen has a single action, **Download and install**.

### Known limitations

- The LibreOffice interface itself is not redesigned yet; this release ships the upstream interface with Chenyi Office branding around it.
- The installer is unsigned. Windows SmartScreen will warn until a code-signing certificate is configured.
- Updates require access to GitHub.

## [0.0.0] — 2026-11-15

Initial project setup.

[0.1.2]: https://github.com/sitong-zhang/office-software/releases/tag/v0.1.2
[0.1.1]: https://github.com/sitong-zhang/office-software/releases/tag/v0.1.1
[0.1.0]: https://github.com/sitong-zhang/office-software/releases/tag/v0.1.0
