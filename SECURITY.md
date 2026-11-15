# Security Policy

## Supported versions

Windows 10/11 x64 builds are published on the Releases page. The latest release receives security fixes; older releases are blocked or asked to upgrade through the mandatory update channel. The current release is **0.1.7**.

The launcher is a mandatory-update client: it accepts an update only from an installer signed by the pinned Chenyi Office certificate, with a matching SHA-256. The install password is a distribution gate for the first installation and is not used for updates; it is not a security boundary.

## Reporting a vulnerability

Please do not report security vulnerabilities through public issues, discussions, or pull requests.

Use GitHub's private vulnerability reporting instead:

1. Open the [Security tab](https://github.com/sitong-zhang/office-software/security).
2. Choose **Report a vulnerability**.
3. Describe the issue, the affected version or commit, and the steps to reproduce it.

If private reporting is unavailable, contact the maintainers through their GitHub profiles and ask for a private channel. Do not include exploit details in a public message.

## What to expect

- Acknowledgement of the report as soon as possible.
- An assessment of severity and impact.
- A fix and coordinated disclosure timeline, agreed with the reporter.
- Credit in the advisory unless you prefer to remain anonymous.

## Scope

This policy covers this repository, its build artifacts, and the project site. Vulnerabilities inherited from upstream LibreOffice should also be reported upstream through [The Document Foundation's security process](https://www.libreoffice.org/about-us/security/); we coordinate with upstream where applicable.
