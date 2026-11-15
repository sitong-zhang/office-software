# Build pipeline

The Windows installer is produced entirely on Linux. No Windows machine or MSVC toolchain is involved.

## Requirements

| Tool | Purpose |
| --- | --- |
| `mingw-w64` | Cross-compiles the launcher, uninstaller, and installer |
| `msitools` (`msiinfo`) | Reads the engine ProductCode for uninstall |
| `python3` + Pillow | Generates the logo, icon, and welcome background |
| `curl` | Downloads the pinned engine MSI |
| `osslsigncode` + `openssl` | Signs binaries and exports the public certificate |
| `librsvg2-bin` (`rsvg-convert`) | Rasterizes the Material Design Icons SVG |

On Ubuntu:

```bash
sudo apt-get install mingw-w64 msitools python3-pil curl osslsigncode openssl librsvg2-bin
```

## Run

```bash
./scripts/build-windows.sh
```

Environment overrides:

- `ENGINE_VERSION` — office engine version (default `26.8.3`)
- `ENGINE_URL` — direct MSI URL (defaults to the Tsinghua mirror)
- `ENGINE_MSI_SHA256` — pinned engine checksum; must be updated together with `ENGINE_VERSION`
- `SIGN_PFX` / `SIGN_CERT` — signing key and public certificate paths
- `SIGN_PFX_PASS` — signing password; alternatively a git-ignored `build/signing/password.txt`
- `REQUIRE_SIGN=1` — fail the build when the certificate or password is missing (CI release builds)

## Pipeline

1. **Assets** — `scripts/make-brand-assets.py`, `make-engine-assets.py`, and `make-ui-icons.py` write the logo, icon, welcome background, installer BMP, splash images, and interface icons.
2. **Editor layout** — `app/engine-ui/` contains the generated ribbon, menu, and Start Center files. Regenerate them with `scripts/make-engine-ui.py --source <extracted soffice.cfg>` after changing the layout rules.
3. **Launcher** — Win32 app with splash, update check, mandatory update screen, announcement screen, and engine launch.
4. **Uninstaller** — removes the engine through its ProductCode, then shell integration and files.
5. **Engine** — downloads the pinned MSI into `build/cache/` and verifies the pinned SHA-256 before use.
6. **Installer shell** — Win32 app with the password-gated welcome page (first install only), installation progress, and finish screen. The launcher, uninstaller, license, notice, splash images, logo/About marks, and generated editor layouts are embedded as resources. After the MSI completes, the installer writes the brand artwork and layouts into an engine it installed itself, hides the engine's own Add/Remove Programs entry, and removes its shortcuts.
7. **Payload** — the MSI is appended to the installer with a 24-byte footer (`CHNYSET1`, offset, size). The installer extracts it to `%TEMP%` at install time.
8. **Output** — `dist/ChenyiOffice-0.3.1-x64.exe`, its SHA-256, and `ChenyiOffice-code-signing.cer`.

## Runtime layout

- Program files: `%ProgramFiles%\ChenyiOffice`
- Engine: the directory created by the engine MSI
- State: `%LOCALAPPDATA%\ChenyiOffice\state.ini`
- Shortcuts: Public Desktop and the common Start Menu

## Installer gate

The welcome page requires the install password only for a fresh install. Re-running the installer over an existing Chenyi Office installation is treated as an update and skips the password; the launcher's silent `/S` path also skips it. The password is stored only as a SHA-256 hash and is distributed separately; it is not documented in this repository. The gate is a distribution control, not encryption — the appended MSI can be extracted by anyone with the installer, so it must not be treated as a security boundary.

## Existing engine

If a compatible engine is already installed and Chenyi Office was not previously installed, the engine is treated as the user's own. The installer skips the MSI, leaves the engine artwork and layouts untouched, records `EnginePreExisting=1`, and the uninstaller does not remove it. The two products share a ProductCode, so a fully isolated side-by-side engine would require an MSI transform with a new ProductCode.

## Release

After building:

1. Create a draft GitHub Release tagged `v0.3.1`.
2. Upload `dist/ChenyiOffice-0.3.1-x64.exe`, its checksum, and the public certificate.
3. Publish the release. `.github/workflows/update-manifest.yml` generates and attaches `latest.json`.

The installer filename must contain `x64` so the manifest workflow can identify it.
