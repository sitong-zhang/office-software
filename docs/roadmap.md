# Roadmap

This roadmap describes the path from project foundation to a stable Windows office suite. Dates are intentionally omitted; milestones are tracked in [GitHub Milestones](https://github.com/sitong-zhang/office-software/milestones) and announced in [Discussions](https://github.com/sitong-zhang/office-software/discussions/categories/announcements).

## Phase 0 — Foundation (current)

- [x] Public repository, licensing, and project documentation
- [x] Product name: Chenyi Office（胡辰逸定制版）
- [x] Contribution, security, and branding policies
- [x] Project site, announcements channel, and release process
- [x] Update manifest workflow and mandatory update specification
- [ ] Windows 10/11 x64 build environment
- [ ] Upstream source integration

## Phase 1 — Windows client and identity

- [ ] Pin the upstream LibreOffice baseline and define the patch series
- [ ] Reproducible Windows x64 build
- [ ] Remove upstream trademarks and product strings from all user-facing surfaces
- [ ] Application icon, window chrome, and file-type icons
- [ ] Splash animation source and exported assets
- [ ] MSI installer, per-user and per-machine variants
- [ ] Authenticode signing and release signing keys
- [ ] License and branding compliance review

## Phase 2 — Interface rebuild

- [ ] Design tokens, typography, and component inventory
- [ ] Start Center redesign
- [ ] Menu, toolbar, and sidebar density pass
- [ ] Dialog and settings reorganization
- [ ] Dark theme, high contrast, and DPI review
- [ ] First preview build for early testers

## Phase 3 — Updater and public beta

- [ ] In-app updater: manifest check, blocking update screen, download and install
- [ ] SHA-256 and Authenticode verification
- [ ] Signed manifest verification
- [ ] Relaunch and version confirmation flow
- [ ] Offline and failed-update handling
- [ ] Public beta with mandatory update channel

## Phase 4 — Stable release

- [ ] Version 1.0 for Windows 10/11 x64
- [ ] Maintenance policy and update cadence
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
