# Branding and trademark rules

Office Software is derived from LibreOffice and must respect the upstream license and trademark policy while presenting a distinct product identity. These rules apply to the application, installers, the project site, and all documentation.

## Product identity

- The working product name is **Office Software**. It is a placeholder and may be replaced before the first public release. The final name must be recorded in this document and in [NOTICE](../NOTICE).
- The name must not include "LibreOffice", "LibreOffice Office", or any confusingly similar variant.
- Do not use the LibreOffice logo, the Document Foundation logo, or upstream splash screens as product identity.
- Do not imply sponsorship, endorsement, or certification by The Document Foundation.

## Required attribution

Attribution is informational and must remain visible but separate from the product name.

- The About dialog includes: "Based on LibreOffice technology. LibreOffice is a trademark of The Document Foundation."
- [NOTICE](../NOTICE) is shipped with every binary distribution.
- The project site footer carries the same statement.
- Upstream copyright and license headers in source files stay intact.

## Surfaces to rebrand

| Surface | Action |
| --- | --- |
| Application icon and window icon | Replace with project artwork |
| Splash screen | Replace; no upstream logo |
| About dialog | New product name, version, attribution line |
| Installer artwork and strings | Project name and artwork only |
| Default templates | Project-branded, no upstream marks |
| File type associations | Product name, neutral descriptions |
| Project site and documentation | Project identity; upstream mentioned only as attribution |

## Artwork

- Store source artwork in `docs/assets/` (or the future `branding/` directory) with editable sources and export settings.
- Provide light and dark variants, plus sizes required by each platform.
- Assets must be original or licensed for redistribution; record the license next to the files.

## Review checklist

Before a release:

- [ ] No upstream trademark appears in product name, window titles, installers, or marketing copy.
- [ ] Attribution line is present in the About dialog, NOTICE, and site footer.
- [ ] Upstream license headers are unchanged.
- [ ] New artwork is original and documented.
- [ ] Screenshots and store listings use the current product identity.
