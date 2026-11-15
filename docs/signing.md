# Code signing

All Windows executables are signed with Authenticode (SHA-256). The current certificate is **self-signed**, because a publicly trusted code-signing certificate requires identity verification by a certificate authority.

## Current certificate

| Field | Value |
| --- | --- |
| Subject | `CN=Chenyi Office (Hu Chenyi Custom Edition), O=Chenyi Office, OU=Custom Edition, C=CN` |
| Type | Self-signed, code signing EKU |
| Validity | 10 years from generation |
| Public certificate | `dist/ChenyiOffice-code-signing.cer` (attached to each release) |
| Private key | `build/signing/chenyi.pfx`, never committed |
| PFX password | Local build only; set `SIGN_PFX_PASS` or a git-ignored `build/signing/password.txt` |

The build script signs three binaries:

1. `ChenyiOffice.exe` (launcher), before it is embedded as a resource.
2. `Uninstall.exe`, before it is embedded.
3. `ChenyiOffice-<version>-x64.exe` (final installer), after the engine payload is appended.

## Verification

On Linux:

```bash
osslsigncode verify -in dist/ChenyiOffice-0.2.0-x64.exe \
  -CAfile build/signing/chenyi.crt
```

Without `-CAfile`, `osslsigncode` reports a failure because the self-signed root is not in the system CA bundle; the signature itself is still intact.

On Windows PowerShell:

```powershell
Get-AuthenticodeSignature .\ChenyiOffice-0.2.0-x64.exe | Format-List
```

A self-signed signature reports `Status: UnknownError` or "Unknown publisher" on a machine that has not trusted the certificate, even though the signature is cryptographically intact.

## Trusting the certificate (testing only)

To test the signed build without CA trust:

```powershell
Import-Certificate -FilePath .\ChenyiOffice-code-signing.cer `
  -CertStoreLocation Cert:\LocalMachine\TrustedPublisher
```

Only do this on machines you control. A self-signed certificate does not protect users from a compromised release channel, and it does not remove SmartScreen warnings for the general public.

## Rotation and replacing the certificate

A trusted certificate (OV/EV or a signing service such as SignPath or Azure Trusted Signing) can be used with:

```bash
SIGN_PFX=/path/to/ca.pfx SIGN_PFX_PASS=... SIGN_CERT=/path/to/ca.crt \
  REQUIRE_SIGN=1 ./scripts/build-windows.sh
```

The launcher pins the signer thumbprint, so switching certificates is a two-step release:

1. Add the new SHA-1 thumbprint to `kExpectedThumbprints` in `app/launcher/main.cpp` while keeping the old one.
2. Build and release that launcher signed with the **old** certificate. Wait until it is installed.
3. Switch the build to the new certificate. Keep the old thumbprint for one more release, then remove it.
4. Update this document, `docs/updater.md`, and the release notes with the new fingerprint.

Pinned certificates are accepted even after expiry (`CERT_E_EXPIRED`), so an expired certificate does not deadlock updates. Revoked certificates are still rejected.

## Security notes

- The private key must never be committed. `build/` is ignored by Git.
- Keep an offline backup of the key; losing it prevents signing updates with a consistent publisher identity.
- The build script never contains a default password and refuses to publish in CI without signing material (`REQUIRE_SIGN=1`).
- CI reads `SIGN_PFX_BASE64` and `SIGN_PFX_PASS` from repository secrets, decodes the PFX, and derives the public certificate from it.
- The launcher pins the signer certificate thumbprint (`FDD3A2D15971DCBAE6CE40C5BAE76605A89062E4`). If the certificate is ever replaced, a new launcher must ship first with the new thumbprint; otherwise updates will be rejected.
- The update manifest hash and the installer signature are independent checks; both are verified by the launcher.
