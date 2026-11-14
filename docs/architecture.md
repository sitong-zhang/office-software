# Architecture

This document describes the shipped v0.1.1 implementation and the planned path to source-level interface work.

## Current implementation

The product is layered so the Window shell can evolve independently of the office engine:

```text
Installer (password gate)
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
- Welcome page with brand artwork, product introduction, password field, and **下一步确认**.
- The password is stored as a SHA-256 hash and is distributed separately.
- The LibreOffice MSI is appended to the installer with a 24-byte footer (`CHNYSET1`, offset, size) and extracted to `%TEMP%` during installation.
- Installs the launcher and uninstaller, runs the MSI silently, writes registry values, creates Public Desktop and common Start Menu shortcuts, and registers an uninstall entry.
- The uninstaller removes the engine through its MSI ProductCode, then the shell integration and files.

The install gate is a distribution control, not encryption. Anyone with the installer can extract the appended MSI.

### Launcher

- Native Win32, static runtime, no external dependencies.
- Splash window; manifest fetch with timeout; blocking update screen with a single **Download and install** action and no exit path.
- Download verification: size, SHA-256 from the manifest, then installer signature once signing is configured.
- Announcements: `announcements.json` with raw GitHub as primary and jsDelivr as fallback; seen ids stored in `%LOCALAPPDATA%\ChenyiOffice\state.ini`.
- Engine discovery through `HKLM\Software\ChenyiOffice\EnginePath`, then standard LibreOffice paths.

### Engine

The engine is the official LibreOffice 26.8.3 Windows x64 MSI, unmodified. Product identity is currently applied at the shell level only: installer, shortcuts, splash, announcements, update flow. Deep interface changes require building from source, which is Phase 2.

## Source strategy (Phase 2)

The upstream tree is several gigabytes. The product repository stays small and builds from a pinned upstream tag plus a patch series:

1. Fetch the pinned upstream revision.
2. Apply patches from `patches/` in order.
3. Build with the platform toolchain.

The Windows engine build itself requires MSVC or a supported MinGW cross-build environment and substantially more compute than the current shell pipeline. This will be documented here before the first patched build.

## Build and packaging

| Target | Artifact | Built on |
| --- | --- | --- |
| Windows 10/11 x64 | `ChenyiOffice-0.1.1-x64.exe` | Linux + MinGW-w64 (custom Win32 installer) |
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

- `.github/workflows/build-installer.yml` rebuilds the installer on demand and on tags.
- `.github/workflows/update-manifest.yml` publishes the update manifest on release.
- `.github/workflows/validate-announcements.yml` validates the announcement feed on change.

Planned additions: document round-trip tests, installer smoke tests on Windows 10 and 11, license and branding checks, and signed builds.

## Update and announcement channels

- **Releases** — installer, checksum, release notes.
- **Update manifest** — `latest.json`, the only version source the client trusts.
- **Announcement feed** — `announcements.json`, shown once at startup.
- **Discussions → Announcements** — human-readable release news.
- **Project site** — download and status summary.
