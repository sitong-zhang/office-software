# Build pipeline

The Windows installer is produced entirely on Linux. No Windows machine or MSVC toolchain is involved.

## Requirements

| Tool | Purpose |
| --- | --- |
| `mingw-w64` | Cross-compiles the launcher, uninstaller, and installer |
| `msitools` (`msiinfo`) | Reads the LibreOffice ProductCode for uninstall |
| `python3` + Pillow | Generates the logo, icon, and welcome background |
| `curl` | Downloads the pinned LibreOffice MSI |

On Ubuntu:

```bash
sudo apt-get install mingw-w64 msitools python3-pil curl
```

## Run

```bash
./scripts/build-windows.sh
```

Environment overrides:

- `LO_VERSION` — LibreOffice engine version (default `26.8.3`)
- `LO_URL` — direct MSI URL (defaults to the Tsinghua mirror)

## Pipeline

1. **Assets** — `scripts/make-brand-assets.py` writes the logo, icon, welcome background, and installer BMP.
2. **Launcher** — Win32 app with splash, update check, mandatory update screen, announcement screen, and engine launch.
3. **Uninstaller** — removes the engine through its ProductCode, then shell integration and files.
4. **Engine** — downloads `LibreOffice_<version>_Win_x86-64.msi` into `build/cache/`.
5. **Installer shell** — Win32 app with the password-gated welcome page, installation progress, and finish screen. The launcher, uninstaller, license, and notice are embedded as resources.
6. **Payload** — the MSI is appended to the installer with a 24-byte footer (`CHNYSET1`, offset, size). The installer extracts it to `%TEMP%` at install time.
7. **Output** — `dist/ChenyiOffice-0.1.2-x64.exe`, its SHA-256, and `ChenyiOffice-code-signing.cer`.

## Runtime layout

- Program files: `%ProgramFiles%\ChenyiOffice`
- Engine: `%ProgramFiles%\LibreOffice`
- State: `%LOCALAPPDATA%\ChenyiOffice\state.ini`
- Shortcuts: Public Desktop and the common Start Menu

## Installer gate

The welcome page requires the install password. The password is stored only as a SHA-256 hash and is distributed separately; it is not documented in this repository. The gate is a distribution control, not encryption — the appended MSI can be extracted by anyone with the installer, so it must not be treated as a security boundary.

## Release

After building:

1. Create a draft GitHub Release tagged `v0.1.2`.
2. Upload `dist/ChenyiOffice-0.1.2-x64.exe`, its checksum, and the public certificate.
3. Publish the release. `.github/workflows/update-manifest.yml` generates and attaches `latest.json`.

The installer filename must contain `x64` so the manifest workflow can identify it.
