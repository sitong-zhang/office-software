# Changelog

All notable user-visible changes to Chenyi Office（胡辰逸定制版） are documented here. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.4.0] — 2026-11-15

### Changed

- The Math editor now receives the same unified ribbon/menu treatment as Writer, Calc, Impress, and Draw.
- Neutralized all remaining third-party product wording in documentation and source comments; public-facing material describes Chenyi Office solely as an independent product.
- The brand profile now sets neutral module display names (文字文档 / 电子表格 / 演示文稿 / 绘图 / 公式 / 主控文档 / HTML 文档) and disables the template picker shown when creating a new presentation.
- Hardcoded engine-compatibility identifiers are consolidated into a single documented block used only to clean up shell artifacts created by the engine installer.
- The Windows smoke workflow now verifies the pinned engine payload checksum, the public signing certificate, and the minimum-supported-version policy.

- The editor About dialog now uses Chenyi Office title, description, copyright, project link, and release-notes link; the credits entry is hidden.
- Fresh installations place the engine in `%ProgramFiles%\ChenyiOffice\Engine`. Existing installations keep their current engine path, so updates do not relocate files.
- The installer passes `INSTALLLOCATION` to the engine MSI for new installations and removes the engine's legacy registry root after a bundled install.
- The Start Center create list and document filter now use neutral Chinese labels (文字文档 / 电子表格 / 演示文稿 / 绘图 / 公式 / 数据库).
- The build downloads a neutrally named engine asset from the project's own release; `engine/SOURCE.md` records the engine version, pinned checksum, modified files, generators, and license information.
- Removed the incompatible-license layout reference and replaced commercial-product wording in public documents with neutral descriptions.
- The launcher handles `WM_DPICHANGED` and resizes/repositions the window and controls; the installer and uninstaller do the same.
- The engine card now shows a **重新安装内核** button when no engine is found.
- The Windows smoke workflow fails on a dirty runner instead of silently skipping, and checks that `latest.json` matches the release tag.

## [0.3.1] — 2026-11-15

### Changed

- Removed the UI builder requirement tag from the generated ribbon files, so the installed UI resources carry neutral metadata.
- Regenerated all four ribbon layouts and the Start Center layout from the updated generator.

## [0.3.0] — 2026-11-15

### Changed

- Rebuilt the editor interface: Writer, Calc, Impress, and Draw now use a grouped tab order (文件 / 开始 / 插入 / 页面布局 / 引用 / 审阅 / 视图 / 工具 / 扩展, with 公式 before 数据 in Calc).
- Added a compact **编辑** group (Find & Replace, Select All) to the Home tab of all four applications.
- Removed the Start Center promotional panel and button, and removed getting-involved, feedback, Q&A, tip-of-the-day, documentation, and about entries from the editor Help menus. The licence entry remains reachable.
- Applied consistent Chenyi Office branding across the launcher, installer, uninstaller, splash, About artwork, README, project site, announcements, and documentation.
- The installer now writes the generated ribbon, menu, and Start Center layouts into engines it installs itself, hides the engine's own Add/Remove Programs entry (`ARPSYSTEMCOMPONENT=1`), removes engine-created Start menu and desktop shortcuts, and renames the temporary payload.
- Removed the promotional entry from the engine About artwork.

### Fixed

- A compatible engine that existed before Chenyi Office is still left completely untouched; the deep UI layout is only applied to the bundled engine.

### Known limitations

- The licence texts required by the Mozilla Public License remain inside the engine directory; they are not exposed in the product interface.

## [0.2.0] — 2026-11-15

### Changed

- Rebuilt the shell UI around a grouped workspace: vertical sidebar navigation, a hero greeting card, quick-create cards for Writer / Calc / Impress / Draw, a recent-documents list, and a right rail with version, announcement, and engine status.
- The launcher now has a real home screen instead of jumping straight into the engine. Sidebar actions: 首页、新建、打开、最近、公告、设置.
- Added working actions: create a document, open an existing file, open recent files, check for updates, view announcements, open the local data folder, clear recent records, and show About information.
- Rebuilt the installer as a two-column wizard with a dark brand sidebar, three-step progress, feature rows, password field, and a bottom-right primary action.
- Rebuilt the uninstaller as a two-column layout with a four-step cleanup list and a completion card.
- Added custom window chrome: rounded corners, DWM shadow/corner hints, drag regions, hover states, minimize/close controls, and disabled close while installing or updating.
- Ported layout structure and design tokens from **Tabler** (42k, MIT), **shadcn/ui** (128k, MIT), and **Fluent UI** (20k, MIT). See `docs/ui-references.md`.
- Added 24 **Lucide** interface icons (25.7k, ISC), vendored with license and pinned commit in `app/branding/third-party/lucide/`.

### Build

- The shell now uses GDI+ and DWM (`-lgdiplus -ldwmapi`); `make-ui-icons.py` rasterizes the Lucide SVGs at build time.

### Known limitations

- The engine's internal Start Center, dialogs, and toolbar remain the stock control layouts; this release rebuilds the Chenyi Office shell, not the engine source.
- No Windows 10/11 hardware test was available; the new UI was verified under Wine and the Windows smoke workflow still covers hash, signature, and the silent-mode guard.

## [0.1.7] — 2026-11-15

### Changed

- Replaced every hand-drawn brand mark with the `description` glyph from **Google Material Design Icons** (54,299 stars, Apache 2.0, pinned commit `10ecc14790612ca8fd164e3f4ffe9c16fc50d5b7`).
- The application icon, installer/uninstaller icons, splash mark, Start Center mark, About mark, installer welcome background, and project-site logo now use that 原厂 glyph. Only rasterization and fill color are local changes; the path geometry is untouched.
- Added `app/branding/third-party/material-design-icons/` with the original SVG, Apache 2.0 license, and a `SOURCE.md` provenance record. `NOTICE`, `docs/branding.md`, and the build requirements were updated accordingly.
- Simplified the Start Center mark to the 原厂 glyph plus "CHENYI OFFICE" on dark ink, removing the low-contrast subtitle and duplicate text that caused ghosting.
- Added a platform line to the About artwork.
- Removed the hand-drawn square "✓" badge from the installer finish page; the page now uses the product icon in the top brand row instead.
- Moved the install percentage above the progress bar so it no longer overlaps the welcome illustration.

### Build

- `librsvg2-bin` (`rsvg-convert`) is now required to rasterize the 原厂 SVG. CI installs it.

## [0.1.6] — 2026-11-15

### Fixed

- Brand profile upgrades now **merge** missing product defaults into the existing `registrymodifications.xcu` instead of overwriting it, so user toolbar, icon, and option changes survive version upgrades.
- The launcher accepts multiple pinned certificate thumbprints and tolerates `CERT_E_EXPIRED` for the pinned self-signed identity, so certificate expiry or rotation can no longer deadlock the mandatory update screen. The rotation procedure is documented.
- Silent mode is parsed from exact command-line arguments (`/S`, `-s`, `--silent`) instead of substring matching, so paths containing `/S` cannot trigger it.
- A failed silent update writes a diagnostic file, and the launcher error screen now shows its path.
- Update redirects are followed one hop at a time with HTTPS, port 443, and host checks before each request; automatic WinHTTP redirects are disabled.
- The host allowlist accepts any `*.githubusercontent.com` subdomain, so GitHub asset-host rotations do not require an emergency release.
- If the configured engine disappears, the launcher offers a one-click link to the download page instead of only reporting a missing engine.
- The uninstaller refuses to start while `soffice`, `soffice.bin`, or `ChenyiOffice.exe` is running, and reports an incomplete cleanup instead of claiming success when files remain locked.
- Version consistency checks now also cover the project site, roadmap, architecture, build, signing, and release documents.

### Changed

- The README and architecture docs no longer claim every installation uses engine 26.8.3; a pre-existing engine is reused and its version may differ.
- The v0.1.4 → v0.1.5/v0.1.6 first hop is documented as relying on HTTPS and SHA-256 because the old launcher predates signature verification.

### Known limitations

- No full Windows install → forced update → restart loop is automated yet; a Windows smoke workflow verifies the published installer's hash, signature, and silent-mode guard.
- The install password remains a distribution gate, not a security boundary.

## [0.1.5] — 2026-11-15

### Fixed

- Updates no longer ask for the install password. The launcher runs the downloaded installer with `/S`; the installer also skips the password page when it detects an existing installation.
- The installer recognizes an office engine that was already on the machine, reuses it without overwriting its splash/About artwork, and records that fact so uninstall never removes the user's own engine.
- The launcher now rejects an update whose downloaded file is not signed by the pinned Chenyi Office certificate, and requires a 64-character SHA-256 instead of silently skipping the check.
- Update downloads are rejected when redirects leave the allowlisted GitHub/CDN hosts, and HTTPS-to-HTTP redirects are disabled.
- `min_supported_version` is enforced; a manifest that points below it is rejected.
- Uninstall now removes `%LOCALAPPDATA%\ChenyiOffice` (profile, seen announcements, logs), so a reinstall starts clean.
- The dead `--uninstall` branch was removed from the installer; uninstalling is handled only by `Uninstall.exe`.
- Version resources now match the product version instead of reporting `0.1.3`.
- The installer and uninstaller scale with the system DPI; the uninstaller manifest declares Per-Monitor V2 awareness.
- Announcement timezone parsing accepts both `+08:00` and `+0800`, and announcements are ordered by publication time.
- Launcher state shared between worker threads and the UI is protected by a lock; the splash status text now reflects the actual check result.
- Only one launcher instance can run at a time.
- The launcher accepts a close request on the splash and announcement screens; the mandatory update screen still cannot be dismissed.

### Changed

- Build script no longer contains a default signing password. It reads `SIGN_PFX_PASS` or a git-ignored `build/signing/password.txt`, and `REQUIRE_SIGN=1` fails the build when signing material is missing.
- The engine MSI is verified against a pinned SHA-256 before use.
- CI installs `osslsigncode`, requires signing secrets, and uploads the public certificate alongside the installer.
- The brand profile is re-applied when the brand version changes (with a `.bak` copy), instead of only on first run.

### Known limitations

- The install password is a distribution gate, not a security boundary; the appended MSI can still be extracted by anyone who obtains the installer.
- A pre-existing engine shares its ProductCode with the bundled engine, so Chenyi Office reuses it rather than installing a fully isolated copy.
- No Windows 10/11 hardware test was available; verification was done with cross-compilation and Wine where possible.

## [0.1.4] — 2026-11-15

### Added

- Engine-level brand artwork applied during installation: custom splash screens (`intro.png`, `intro-highres.png`) and new Start Center / About marks (`logo.svg`, `logo-sc.svg`, `about.svg`).
- **Sifr** icon theme as the default interface icon set.
- Installer step that writes brand artwork into the installed engine.

### Changed

- Interface now combines the Tabbed notebookbar, Sifr icons, and Chenyi Office splash and marks.

### Known limitations

- Notification icons, Windows file-type icons, and dialog layouts still come from the stock engine.
- Source-level UI rebuild remains the next track.

## [0.1.3] — 2026-11-15

### Added

- Branded engine interface profile with the Tabbed ribbon for Writer, Calc, Impress, and Draw.
- Product name and vendor set to Chenyi Office inside the office engine.
- Dedicated profile in `%LOCALAPPDATA%\ChenyiOffice\lo-profile`, launched with `-env:UserInstallation`.
- First-run tips, What's New, and promotional infobars disabled.

### Changed

- Launcher prepares the brand profile before starting the engine.
- Profile settings are applied only on first run, preserving later user customization.

### Known limitations

- Dialogs, widgets, and the Start Center still use 原厂 控件层 layouts; source-level redesign is the next step.

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

- Windows 10/11 x64 installer built on Linux with MinGW-w64, bundling engine 26.8.3.
- Branded welcome page with a background image, logo, product introduction, install-password gate, and a **下一步确认** action.
- Launcher with a custom splash animation, update check, blocking update screen (no exit or skip), in-app announcements, and engine start.
- Brand identity: logo, application icon, installer artwork.
- In-app announcement feed (`announcements.json`) with validation workflow.
- Uninstaller and shell integration (shortcuts, registry entries).

### Changed

- Product named **Chenyi Office（胡辰逸定制版）**.
- Update screen has a single action, **Download and install**.

### Known limitations

- The engine interface itself is not redesigned yet; this release ships the stock interface with Chenyi Office branding around it.
- The installer is unsigned. Windows SmartScreen will warn until a code-signing certificate is configured.
- Updates require access to GitHub.

## [0.0.0] — 2026-11-15

Initial project setup.

[0.4.0]: https://github.com/sitong-zhang/office-software/releases/tag/v0.4.0
[0.3.1]: https://github.com/sitong-zhang/office-software/releases/tag/v0.3.1
[0.3.0]: https://github.com/sitong-zhang/office-software/releases/tag/v0.3.0
[0.2.0]: https://github.com/sitong-zhang/office-software/releases/tag/v0.2.0
[0.1.7]: https://github.com/sitong-zhang/office-software/releases/tag/v0.1.7
[0.1.6]: https://github.com/sitong-zhang/office-software/releases/tag/v0.1.6
[0.1.5]: https://github.com/sitong-zhang/office-software/releases/tag/v0.1.5
[0.1.4]: https://github.com/sitong-zhang/office-software/releases/tag/v0.1.4
[0.1.3]: https://github.com/sitong-zhang/office-software/releases/tag/v0.1.3
[0.1.2]: https://github.com/sitong-zhang/office-software/releases/tag/v0.1.2
[0.1.1]: https://github.com/sitong-zhang/office-software/releases/tag/v0.1.1
[0.1.0]: https://github.com/sitong-zhang/office-software/releases/tag/v0.1.0
