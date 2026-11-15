# Branding rules

These rules apply to the application, installers, the project site, and all documentation.

## Product identity

- **Product name:** Chenyi Office（胡辰逸定制版）
- **English display:** Chenyi Office — Hu Chenyi Custom Edition
- **Short name in UI chrome:** Chenyi Office
- The full name, including the parenthesized edition suffix, is used on the splash, About dialog, installer, Start Center, and all user-facing download pages.
- Do not use another product's name, logo, or splash screens as product identity.
- User-facing surfaces describe Chenyi Office as an independent product with its own name, artwork, and interface.

## Icon source

All product icons come from a high-star open-source icon repository. This
project does not draw or generate its own icon shapes.

- Repository: [google/material-design-icons](https://github.com/google/material-design-icons)
- Icon: `description` (`src/action/description/materialicons/24px.svg`)
- Pinned commit: `10ecc14790612ca8fd164e3f4ffe9c16fc50d5b7`
- License: Apache License 2.0
- Local copy and attribution: `app/branding/third-party/material-design-icons/`
- Modification: rasterized to PNG/ICO and tinted with the product palette; path geometry unchanged.

## Notices

- [NOTICE](../NOTICE) is shipped with every binary distribution and records the third-party icon licenses and the MPL source-availability statement.
- Copyright and license headers in source files stay intact.
- The About dialog shows the product name, edition, and platform; the licence entry in the editor Help menu remains reachable.

## Surfaces to rebrand

| Surface | Action |
| --- | --- |
| Application icon and window icon | Material Design Icons `description` glyph (Apache 2.0) |
| Splash screen | Branded startup animation with the product name and edition suffix |
| About dialog | Product name, edition, platform |
| Installer artwork and strings | Product name and artwork only |
| Start Center | Product name, project templates, no third-party marks or promotional panel |
| Default templates | Project-branded, no third-party marks |
| File type associations | Product name, neutral descriptions |
| Update screen | Product name; version and release information only |
| Project site and documentation | Product identity only |

## Artwork

- Store source artwork in `docs/assets/` (or the future `branding/` directory) with editable sources and export settings.
- Provide light and dark variants, plus sizes required by Windows (16–256 px), the installer, and the site.
- Assets must be original or licensed for redistribution; record the license next to the files.
- Icons must come from a high-star open-source repository. Do not hand-draw or generate new icon shapes; record the repository, pinned commit, and license in `app/branding/third-party/`.

## Review checklist

Before a release:

- [ ] No third-party product name, logo, or vendor string appears in user-facing screens, documentation, or marketing copy.
- [ ] The full product name appears correctly, including the edition suffix.
- [ ] NOTICE ships with the installer and records all third-party licenses.
- [ ] Source copyright and license headers are unchanged.
- [ ] New artwork is original or third-party licensed, and its source, commit, and license are documented.
- [ ] Screenshots and store listings use the current product identity.
