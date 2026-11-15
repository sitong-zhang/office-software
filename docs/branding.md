# Branding and trademark rules

Chenyi Office（胡辰逸定制版） is derived from LibreOffice and must respect the upstream license and trademark policy while presenting a distinct product identity. These rules apply to the application, installers, the project site, and all documentation.

## Product identity

- **Product name:** Chenyi Office（胡辰逸定制版）
- **English display:** Chenyi Office — Hu Chenyi Custom Edition
- **Short name in UI chrome:** Chenyi Office
- The full name, including the parenthesized edition suffix, is used on the splash, About dialog, installer, Start Center, and all user-facing download pages.
- The name must not include "LibreOffice", "LibreOffice Office", or any confusingly similar variant.
- Do not use the LibreOffice logo, the Document Foundation logo, or upstream splash screens as product identity.
- Do not imply sponsorship, endorsement, or certification by The Document Foundation.

## Icon source

All product icons come from a high-star open-source icon repository. This
project does not draw or generate its own icon shapes.

- Repository: [google/material-design-icons](https://github.com/google/material-design-icons)
- Icon: `description` (`src/action/description/materialicons/24px.svg`)
- Pinned commit: `10ecc14790612ca8fd164e3f4ffe9c16fc50d5b7`
- License: Apache License 2.0
- Local copy and attribution: `app/branding/third-party/material-design-icons/`
- Modification: rasterized to PNG/ICO and tinted with the product palette; path geometry unchanged.

## Required attribution

Attribution is informational and must remain visible but separate from the product name.

- The About dialog includes: "Based on LibreOffice technology. LibreOffice is a trademark of The Document Foundation."
- [NOTICE](../NOTICE) is shipped with every binary distribution.
- The project site footer carries the same statement.
- Upstream copyright and license headers in source files stay intact.

## Surfaces to rebrand

| Surface | Action |
| --- | --- |
| Application icon and window icon | Material Design Icons `description` glyph (Apache 2.0); no upstream LibreOffice logo |
| Splash screen | Branded startup animation with the product name and edition suffix |
| About dialog | Product name, version, attribution line |
| Installer artwork and strings | Product name and artwork only |
| Start Center | Product name, project templates, no upstream marks |
| Default templates | Project-branded, no upstream marks |
| File type associations | Product name, neutral descriptions |
| Update screen | Product name; version and release information only |
| Project site and documentation | Project identity; upstream mentioned only as attribution |

## Artwork

- Store source artwork in `docs/assets/` (or the future `branding/` directory) with editable sources and export settings.
- Provide light and dark variants, plus sizes required by Windows (16–256 px), the installer, and the site.
- Assets must be original or licensed for redistribution; record the license next to the files.
- Icons must come from a high-star open-source repository. Do not hand-draw or generate new icon shapes; record the upstream repository, pinned commit, and license in `app/branding/third-party/`.

## Review checklist

Before a release:

- [ ] No upstream trademark appears in product name, window titles, installers, or marketing copy.
- [ ] The full product name appears correctly, including the edition suffix.
- [ ] Attribution line is present in the About dialog, NOTICE, and site footer.
- [ ] Upstream license headers are unchanged.
- [ ] New artwork is original or third-party licensed, and its source, commit, and license are documented.
- [ ] Screenshots and store listings use the current product identity.
