# Roadmap

Current release: **v0.3.0** — Windows 10/11 x64 preview with the Chenyi Office launcher, installer, rebuilt editor ribbon, passwordless updates, mandatory update gate, and announcement channel.

Dates are intentionally omitted; milestones are tracked in [GitHub Milestones](https://github.com/sitong-zhang/office-software/milestones) and announced in [Discussions](https://github.com/sitong-zhang/office-software/discussions/categories/announcements).

## Phase 0 — Foundation (done)

- [x] Public repository, licensing, and project documentation
- [x] Product name: Chenyi Office（胡辰逸定制版）
- [x] Contribution, security, and branding policies
- [x] Project site, announcements channel, release process
- [x] Update manifest workflow and mandatory update specification
- [x] In-app announcement feed and validation workflow

## Phase 1 — Windows client and identity (done)

- [x] Linux cross-build pipeline (MinGW-w64, no Windows machine)
- [x] Pinned engine: 26.8.3 Windows x64
- [x] Password-gated welcome page with branded artwork
- [x] Custom splash animation
- [x] Installer with engine payload, shortcuts, registry, and uninstaller
- [x] Brand identity: logo, icon, installer background
- [x] Authenticode signing (self-signed certificate; CA certificate pending)
- [x] Authenticated updates (mandatory SHA-256 + pinned Authenticode publisher; manifest treated as an untrusted hint)

## Phase 2 — Interface rebuild (in progress)

- [x] Branded engine profile (Tabbed ribbon, product name, clean first run)
- [x] Dedicated user profile launched with `-env:UserInstallation`
- [x] Remove tip, What's New, feedback, and getting-involved entries
- [x] WPS-style tab order and compact Home groups for all four applications
- [x] Start Center without the promotional panel
- [x] Engine artwork without third-party product references
- [ ] Dialog and settings reorganization
- [ ] Dark theme, high contrast, and DPI review
- [ ] Interface preview build

## Phase 3 — Beta

- [ ] Localization of the interface and documentation
- [ ] Crash reporting and feedback workflow
- [ ] Accessibility review
- [ ] Public beta with the mandatory update channel

## Phase 4 — Stable release

- [ ] Version 1.0 for Windows 10/11 x64
- [ ] Maintenance policy and release cadence
- [ ] Release notes archive and update history

## Not planned

- macOS and Linux builds
- Mobile and web editions
- Portable builds

## Guiding principles

1. **Interface first.** Every release should make the product easier to look at and easier to operate.
2. **Compatibility.** Documents must open, render, and round-trip without surprises.
3. **A smaller, clearer surface.** Curated defaults beat an overwhelming feature list.
4. **Updates are part of the product.** Users always run the latest build; the update screen is honest, fast, and unavoidable.
5. **Craft.** Spacing, typography, icons, and wording are part of the product, not decoration.
