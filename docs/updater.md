# Update protocol

Every launch checks this repository for a newer build. When a new version has been published, the launcher shows an optional update prompt: the user can install it now, choose to be reminded later, or close the window. The current build stays fully usable until an update is chosen. Updates never interrupt work.

## Endpoint

```text
https://github.com/sitong-zhang/office-software/releases/latest/download/latest.json
```

The file is attached to every release by the workflow in `.github/workflows/update-manifest.yml`. Because the URL uses `releases/latest`, it always points at the newest non-prerelease release.

## Manifest schema

```json
{
  "version": "1.2.0",
  "mandatory": false,
  "min_supported_version": "0.0.0",
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
| `mandatory` | Always `false`. Retained for manifest compatibility; it never blocks the user. See [Compatibility with clients before 0.4.1](#compatibility-with-clients-before-041). |
| `min_supported_version` | Builds below this are offered an update even when `version` is not newer |
| `installer.url` | Direct asset URL on the repository host |
| `installer.sha256` | Required; the download is rejected on mismatch |
| `installer.size` | Used for the progress display and a sanity check |

`latest.json` is a version hint, not a command. Integrity is enforced on the downloaded installer, and the decision to install always rests with the user.

## Launch sequence

1. Splash starts immediately and renders the update-check state.
2. The client makes one request to the manifest with 8 s connect/send/receive timeouts and a 12 s total timeout, plus `Cache-Control: no-cache`. There is no automatic retry loop; the next launch retries.
3. The announcement feed is fetched on the same check, even when an update is available, so deferring an update does not hide release notices (see [announcements.md](announcements.md)).
4. If the remote `version` is greater than the running version, or the running version is below `min_supported_version`, the splash transitions to the **optional update prompt**. Otherwise the application opens normally, after any unseen announcements.

## Optional update prompt

- Full-window but never blocking: a title-bar close button, `Esc`, and a reminder action are all available.
- Shows current version, target version, download size, and progress.
- Actions: **立即更新** (Update now) and **稍后提醒** (Remind me later); the Chinese labels are the ones shown in the product UI.
- Deferring or closing leaves the running build fully usable. The check runs again on the next launch, so the offer is repeated but never forced. Unseen announcements are still shown before the home screen.
- Download, verification and installation screens are also closable; leaving the launcher simply defers the update.
- After a chosen update installs, the app relaunches and re-runs the check.

## Download, verify, install

1. Download to a temporary directory.
2. Verify size, then SHA-256 against the manifest. A missing or mismatched hash is a failure, never a skip.
3. Verify the Authenticode signature of the installer and require it to be signed by a pinned Chenyi Office certificate. A self-signed certificate is expected; the pinned thumbprint, not the Windows trust store, is the identity check. Multiple thumbprints may be compiled in for rotation, and `CERT_E_EXPIRED` is accepted for a pinned identity.
4. Run the installer silently (`/S` for the EXE wrapper). Silent mode is reserved for updates: the installer refuses it when Chenyi Office is not already installed, and it never asks for the install password. After installation the launcher relaunches.
5. If any step fails, show the retry screen with the path of `%TEMP%\ChenyiOffice-update-error.txt`. The retry screen also offers the reminder action and a close button, so a failed update never traps the user.

## Offline and failure behavior

- If the manifest cannot be fetched (offline, DNS failure, timeout), the splash shows "无法检查更新" and the app opens without interruption. The next launch retries.
- An update can be deferred indefinitely; there is no deadline and no forced install.
- An update cannot be deferred by changing the system clock; the running version is compared to the manifest, not to time.

## Compatibility with clients before 0.4.1

Launchers up to and including 0.4.0 shipped with a blocking update screen. Those builds read the manifest but decided on their own that an update was mandatory; they ignore the `mandatory` field. Publishing 0.4.1 is therefore the last delivery through that older blocking flow: users still on 0.4.0 are upgraded to 0.4.1 by their own launcher. From 0.4.1 onward every client offers updates as described here, and no release marks itself mandatory.

## Security requirements

- HTTPS only. Automatic redirects are disabled; the client follows `Location` one hop at a time (at most five hops) and validates scheme, port 443, and host before every request. Allowed hosts are `github.com`, `*.githubusercontent.com`, and `cdn.jsdelivr.net`.
- `installer.sha256` is required and must be 64 hex characters. An empty or malformed value rejects the manifest.
- The downloaded installer must carry an Authenticode signature whose signer certificate thumbprint matches the value compiled into the launcher. This is the trust anchor: a tampered manifest cannot make the client run an unsigned binary.
- The manifest itself is not separately signed. It is treated as an untrusted hint; integrity is enforced on the downloaded envelope instead.
- Downgrades are never installed: the remote version must be greater than the running version (or the running version must be below `min_supported_version`), and a manifest whose `min_supported_version` is greater than its `version` is rejected. A rollback is published as a higher version number.
- The updater runs with the same integrity level as the application and requests elevation only when launching the installer.

## First hop from v0.1.4

v0.1.4 shipped before signature verification existed in the launcher. Its first update to a later version is therefore protected only by HTTPS and the SHA-256 from `latest.json`. From the v0.1.5 launcher onward, every update also requires the pinned Authenticode publisher. Users who want certificate-level assurance for that first hop can download the installer manually and compare both the SHA-256 and the certificate thumbprint (`FDD3A2D15971DCBAE6CE40C5BAE76605A89062E4`).

## Certificate lifecycle

- The launcher accepts one or more pinned SHA-1 thumbprints.
- For rotation: ship a launcher containing both the old and the new thumbprint while the old certificate still signs releases. After that launcher is widely installed, switch the signing certificate and keep the old thumbprint for one release.
- Expired certificates are accepted only when the signer thumbprint matches a pin, so expiry cannot stall the update prompt.
- A revoked certificate is not accepted.

## Release-side automation

`.github/workflows/update-manifest.yml` runs when a release is published:

- selects the `ChenyiOffice-<version>-x64.exe` installer asset by name, so the bundled engine MSI is never used as an update payload;
- computes SHA-256 and size;
- writes `latest.json` with `mandatory: false` and `min_supported_version` from the repository variable `MIN_SUPPORTED_VERSION` (defaults to `0.0.0`);
- uploads it to the release, replacing any previous copy.

If a release has no matching installer, the workflow exits without publishing a manifest so clients are never pointed at a missing file. `windows-smoke.yml` additionally asserts that the published manifest is not marked mandatory.

## Acceptance criteria

- [x] Fresh install checks on every launch.
- [x] Published newer release offers an update within one launch.
- [x] Update prompt can be deferred or closed and leaves the current build usable.
- [x] Tampered download and unsigned installer are rejected and surface a retry.
- [x] Updates never ask for the install password.
- [x] Published installer hash, signature, and silent-mode guard verified on a Windows runner (`windows-smoke.yml`).
- [ ] Full install → optional update → restart loop verified on Windows 10 and 11 hardware.
- [ ] Relaunch after install reports the new version on Windows hardware.
