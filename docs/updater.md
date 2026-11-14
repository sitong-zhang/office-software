# Update protocol and mandatory updates

Every launch checks this repository for a newer build. When a new version has been published, the application shows a blocking update screen and does not continue until the update is installed. The update screen cannot be dismissed or skipped.

## Endpoint

```text
https://github.com/sitong-zhang/office-software/releases/latest/download/latest.json
```

The file is attached to every release by the workflow in `.github/workflows/update-manifest.yml`. Because the URL uses `releases/latest`, it always points at the newest non-prerelease release.

## Manifest schema

```json
{
  "version": "1.2.0",
  "mandatory": true,
  "min_supported_version": "1.0.0",
  "published_at": "2026-12-01T08:00:00Z",
  "release_notes_url": "https://github.com/sitong-zhang/office-software/releases/tag/v1.2.0",
  "installer": {
    "url": "https://github.com/sitong-zhang/office-software/releases/download/v1.2.0/ChenyiOffice-1.2.0-x64.exe",
    "sha256": "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    "size": 348192768
  },
  "notes": "Short summary shown on the update screen."
}
```

| Field | Meaning |
| --- | --- |
| `version` | Latest version, compared with the running build |
| `mandatory` | Always `true` for this product; a `false` value is ignored |
| `min_supported_version` | Builds below this are blocked even if they equal `version` |
| `installer.url` | Direct asset URL on the repository host |
| `installer.sha256` | Required; the download is rejected on mismatch |
| `installer.size` | Used for the progress display and a sanity check |

## Launch sequence

1. Splash starts immediately and renders the update-check state.
2. The client requests the manifest with a 10 s timeout, up to 3 attempts, and `Cache-Control: no-cache`.
3. If the remote `version` is greater than the running version, or the running version is below `min_supported_version`, the splash transitions to the **mandatory update screen**.
4. Otherwise the application opens normally.

## Mandatory update screen

- Full-window, modal, and blocking; no close button, no Esc, no skip link.
- Shows current version, target version, release notes summary, download size, and progress.
- Actions: **Download and install** (primary), **Exit** (secondary). There is no "Remind me later".
- The window cannot be closed until the installer starts; closing the app during download is allowed only through **Exit**, which discards the partial download.
- After installation, the app relaunches and re-runs the check.

## Download, verify, install

1. Download to a temporary directory if it has enough free space (size × 3).
2. Verify size, then SHA-256 against the manifest. A mismatch deletes the file and offers a retry.
3. Verify the Authenticode signature of the installer and that the publisher matches the expected certificate.
4. Run the installer silently (`/quiet` for MSI, `/S` for the EXE wrapper), then relaunch the app.
5. If any step fails, show the manual download link and a retry; the app remains blocked.

## Offline and failure behavior

- If the manifest cannot be fetched (offline, DNS failure, timeout), the app may open with a persistent "update check unavailable" notice and rechecks every 30 minutes.
- Once a newer version has been confirmed, the block applies. It is not possible to continue on the old build.
- An update cannot be deferred by changing the system clock; the running version is compared to the manifest, not to time.

## Security requirements

- HTTPS only, with the repository host pinned; redirects must stay on GitHub domains.
- Manifest signature verification (Ed25519 or minisign) is required before the first public release; the public key ships with the application.
- Installs are rejected if the signature, hash, or publisher does not match.
- Downgrades are never installed; an emergency rollback is published as a higher version number.
- The updater runs with the same integrity level as the application and never requests elevation beyond what the installer requires.

## Release-side automation

`.github/workflows/update-manifest.yml` runs when a release is published:

- locates the x64 installer asset (`.exe` or `.msi` matching `x64`);
- computes SHA-256 and size;
- writes `latest.json` with `mandatory: true` and `min_supported_version` from the repository variable `MIN_SUPPORTED_VERSION` (defaults to `0.0.0`);
- uploads it to the release, replacing any previous copy.

If a release has no matching installer, the workflow exits without publishing a manifest so clients are never pointed at a missing file.

## Acceptance criteria

- [ ] Fresh install checks on every launch.
- [ ] Published newer release blocks the app within one launch.
- [ ] Update screen has no dismissal path.
- [ ] Tampered download is rejected and surfaces a retry.
- [ ] Installer elevation and silent-install flags verified on Windows 10 and 11.
- [ ] Relaunch after install reports the new version.
