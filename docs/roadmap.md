# Roadmap

Current release: **v0.1.2** — Windows 10/11 x64 preview with the Chenyi Office launcher, installer, splash, mandatory update gate, and announcement channel. The LibreOffice interface itself is still upstream.

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
- [x] Pinned engine: LibreOffice 26.8.3 Windows x64
- [x] Password-gated welcome page with branded artwork
- [x] Custom splash animation
- [x] Installer with engine payload, shortcuts, registry, and uninstaller
- [x] Brand identity: logo, icon, installer background
- [x] Authenticode signing (self-signed certificate; CA certificate pending)
- [ ] Signed update manifests

## Phase 2 — Interface rebuild (in progress)

- [ ] Upstream source integration and patch series
- [ ] Design tokens, typography, and component inventory
- [ ] Start Center redesign
- [ ] Menu, toolbar, and sidebar density pass
- [ ] Dialog and settings reorganization
- [ ] Dark theme, high contrast, and DPI review
- [ ] Interface preview build

## Phase 3 — Beta

- [ ] Engine-level rebranding (icons, splash, About, defaults)
- [ ] Localization of the interface and documentation
- [ ] Crash reporting and feedback workflow
- [ ] Accessibility review
- [ ] Public beta with the mandatory update channel

## Phase 4 — Stable release

- [ ] Version 1.0 for Windows 10/11 x64
- [ ] Maintenance policy and release cadence
- [ ] Release notes archive and update history

## Out of scope until 1.0

- macOS and Linux builds
- Mobile and web editions
- Portable builds

## Guiding principles

1. **Interface first.** Every release should make the product easier to look at and easier to operate.
2. **Compatibility.** Documents must open, render, and round-trip without surprises.
3. **A smaller, clearer surface.** Curated defaults beat an overwhelming feature list.
4. **Updates are part of the product.** Users always run the latest build; the update screen is honest, fast, and unavoidable.
5. **Upstream respect.** Improvements that make sense upstream are offered back.
