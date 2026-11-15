# Bundled office engine: source and license

Chenyi Office bundles a Windows x64 office engine (version 26.8.3). This file
records the engine's license and source-availability information. It is a
legal/provenance record, not a product-facing page.

## Engine package

- Engine version: 26.8.3
- Build input: `ChenyiEngine-26.8.3-x64-branded.msi` (attached to the v0.4.0 release)
- Pinned SHA-256: `223469a23d78b9a8ca900a985ee5205a15b29da2b497d0800ca075303ef47fb6`
- License: Mozilla Public License 2.0

The engine MSI is attached to this project's release so builds do not depend
on any third-party mirror. The attached payload carries the product's own
identity: its installer tables, setup UI, feature names, shortcuts and bundled
resources all use Chenyi Office branding. The attached file is verified against
the pinned SHA-256 before use.

## Customized engine resources

The installer copies these generated resources into the engine tree after the
MSI completes (bundled engine only; a pre-existing engine is never altered):

- `share/config/soffice.cfg/modules/swriter/ui/notebookbar.ui`
- `share/config/soffice.cfg/modules/swriter/menubar/menubar.xml`
- `share/config/soffice.cfg/modules/scalc/ui/notebookbar.ui`
- `share/config/soffice.cfg/modules/scalc/menubar/menubar.xml`
- `share/config/soffice.cfg/modules/simpress/ui/notebookbar.ui`
- `share/config/soffice.cfg/modules/simpress/menubar/menubar.xml`
- `share/config/soffice.cfg/modules/sdraw/ui/notebookbar.ui`
- `share/config/soffice.cfg/modules/sdraw/menubar/menubar.xml`
- `share/config/soffice.cfg/modules/smath/menubar/menubar.xml`
- `share/config/soffice.cfg/sfx/ui/startcenter.ui`
- `share/config/soffice.cfg/cui/ui/aboutdialog.ui`
- `program/intro.png`, `program/intro-highres.png`
- `program/shell/logo.svg`, `program/shell/logo-sc.svg`, `program/shell/about.svg`

The generated files are committed under `app/engine-ui/` and are produced by
`scripts/make-engine-ui.py`. The script records the transformations (tab order,
Home group, menu pruning, Start Center labels, About dialog labels). The
artwork is produced by `scripts/make-engine-assets.py`.

## Source Code Form

The Source Code Form for the customized engine resources consists of:

1. the engine source at the pinned version and license above, and
2. the generated files under `app/engine-ui/` and the generators under
   `scripts/`.

The generators and generated resources are available in this public
repository. Files retain their original MPL 2.0 headers and notices.
