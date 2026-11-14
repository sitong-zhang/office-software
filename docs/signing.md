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
| PFX password | Local build only; override with `SIGN_PFX_PASS` |

The build script signs three binaries:

1. `ChenyiOffice.exe` (launcher), before it is embedded as a resource.
2. `Uninstall.exe`, before it is embedded.
3. `ChenyiOffice-<version>-x64.exe` (final installer), after the engine payload is appended.

## Verification

On Linux:

```bash
osslsigncode verify -in dist/ChenyiOffice-0.1.2-x64.exe
```

On Windows PowerShell:

```powershell
Get-AuthenticodeSignature .\ChenyiOffice-0.1.2-x64.exe | Format-List
```

A self-signed signature reports `Status: UnknownError` or "Unknown publisher" on a machine that has not trusted the certificate, even though the signature is cryptographically intact.

## Trusting the certificate (testing only)

To test the signed build without CA trust:

```powershell
Import-Certificate -FilePath .\ChenyiOffice-code-signing.cer `
  -CertStoreLocation Cert:\LocalMachine\TrustedPublisher
```

Only do this on machines you control. A self-signed certificate does not protect users from a compromised release channel, and it does not remove SmartScreen warnings for the general public.

## Replacing it with a CA certificate

When a trusted certificate (OV/EV or a signing service such as SignPath or Azure Trusted Signing) is available:

```bash
SIGN_PFX=/path/to/ca.pfx SIGN_PFX_PASS=... SIGN_CERT=/path/to/ca.crt \
  ./scripts/build-windows.sh
```

No source changes are required. After switching, update this document and the fingerprint in the release notes.

## Security notes

- The private key must never be committed. `build/` is ignored by Git.
- Keep an offline backup of the key; losing it prevents signing updates with a consistent publisher identity.
- The update manifest hash and the installer signature are independent checks; both are verified by the launcher.
