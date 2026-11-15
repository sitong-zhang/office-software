# Architecture

This document describes the shipped v0.1.7 implementation and the planned path to source-level interface work.

## Current implementation

The product is layered so the Window shell can evolve independently of the office engine:

```text
Installer (password gate for first install)
   └─ installs
      ├─ Chenyi Office launcher  (%ProgramFiles%\ChenyiOffice\ChenyiOffice.exe)
      └─ LibreOffice 26.8.3 engine (%ProgramFiles%\LibreOffice)
Launcher
   ├─ splash animation
   ├─ update check (latest.json) → blocking update installer
   ├─ announcement check (announcements.json) → continue screen
   └─ starts soffice.exe
```

### Installer

- Native Win32 application cross-compiled on Linux.
- Welcome page with brand artwork, product introduction, password field, and **下一步确认**. The password is requested only for a fresh install; re-running the installer over an existing installation switches to an update screen without a password.
- The password is stored as a SHA-256 hash and is distributed separately.
- `/S` (or `--silent`) runs the update path with no UI and no password. It refuses to run when Chenyi Office is not already installed, so a fresh install cannot bypass the gate.
- The LibreOffice MSI is appended to the installer with a 24-byte footer (`CHNYSET1`, offset, size) and extracted to `%TEMP%` during installation.
- Installs the launcher and uninstaller, runs the MSI silently, writes registry values, creates Public Desktop and common Start Menu shortcuts, and registers an uninstall entry.
- If a LibreOffice engine already exists before Chenyi Office, the MSI is skipped, engine artwork is left untouched, and `EnginePreExisting=1` is recorded. The uninstaller then removes only Chenyi Office and keeps the user's engine.
- The uninstaller removes the engine through its MSI ProductCode when Chenyi Office installed it, then the shell integration, `%LOCALAPPDATA%\ChenyiOffice`, and program files.

The install gate is a distribution control, not encryption. Anyone with the installer can extract the appended MSI.

### Launcher

- Native Win32, static runtime, no external dependencies.
- Splash window; manifest fetch with timeout; blocking update screen with a single **Download and install** action and no exit path.
- Download verification: size, mandatory SHA-256 from the manifest, then Authenticode verification against a pinned publisher certificate (multiple thumbprints are accepted for rotation; expired pinned certificates are tolerated). Redirects are followed manually, one hop at a time, and every hop must be HTTPS on port 443 at `github.com`, `*.githubusercontent.com`, or `cdn.jsdelivr.net`.
- Announcements: `announcements.json` with raw GitHub as primary and jsDelivr as fallback; seen ids stored in `%LOCALAPPDATA%\ChenyiOffice\state.ini`.
- Engine discovery through `HKLM\Software\ChenyiOffice\EnginePath`, then standard LibreOffice paths.

### Interface layer

The launcher embeds a `registrymodifications.xcu` template and writes it to `%LOCALAPPDATA%\ChenyiOffice\lo-profile\user` on first start. When the brand version changes, it merges only missing product keys into the existing file (keeping the previous file as `.bak`); existing user values are never overwritten. The engine is then launched with `-env:UserInstallation=file:///...`, which isolates the product profile from any other LibreOffice installation. The template selects the Tabbed notebookbar for all four applications, sets the Sifr icon theme, sets the product name to Chenyi Office, and disables upstream tips and promotional infobars.

The installer also writes engine brand artwork after the MSI completes: custom splash images and Start Center / About marks. This changes the visible engine chrome without modifying the MSI database.

### Engine

When no engine is present, the engine is the official LibreOffice 26.8.3 Windows x64 MSI, unmodified. If a LibreOffice installation already exists, it is reused instead and its version may differ. Product identity and the interface layout are applied through the dedicated profile. Deep visual changes to dialogs and widgets require building from source, which is the next Phase 2 step.

## Source strategy (Phase 2)

The upstream tree is several gigabytes. The product repository stays small and builds from a pinned upstream tag plus a patch series:

1. Fetch the pinned upstream revision.
2. Apply patches from `patches/` in order.
3. Build with the platform toolchain.

The Windows engine build itself requires MSVC or a supported MinGW cross-build environment and substantially more compute than the current shell pipeline. This will be documented here before the first patched build.

## Build and packaging

| Target | Artifact | Built on |
| --- | --- | --- |
| Windows 10/11 x64 | `ChenyiOffice-0.1.7-x64.exe` | Linux + MinGW-w64 (custom Win32 installer) |
| Windows 10/11 x64 | `Uninstall.exe` | Same pipeline |
| Windows 10/11 x64 | Launcher `ChenyiOffice.exe` | Same pipeline |

See [build.md](build.md) for the pipeline and commands.

## Update pipeline

1. A release is published with the x64 installer attached.
2. `.github/workflows/update-manifest.yml` computes SHA-256 and size, writes `latest.json`, and attaches it to the release.
3. The launcher fetches `releases/latest/download/latest.json` at startup.
4. A newer version triggers the blocking update screen.

Protocol details: [updater.md](updater.md).

## Continuous integration

- `.github/workflows/build-installer.yml` rebuilds the installer on demand and on tags; signing secrets are mandatory, so an unsigned artifact is never produced.
- `.github/workflows/update-manifest.yml` publishes the update manifest on release.
- `.github/workflows/validate-announcements.yml` validates the announcement feed on change.
- `.github/workflows/windows-smoke.yml` verifies a published installer on Windows: manifest hash and size, Authenticode signer, and the silent fresh-install guard.

Planned additions: a full install → forced update → restart loop on Windows 10/11 hardware, and license/branding checks.

## Update and announcement channels

- **Releases** — installer, checksum, release notes.
- **Update manifest** — `latest.json`, treated as an untrusted version hint; the downloaded installer is authenticated by its signature.
- **Announcement feed** — `announcements.json`, shown once at startup.
- **Discussions → Announcements** — human-readable release news.
- **Project site** — download and status summary.
