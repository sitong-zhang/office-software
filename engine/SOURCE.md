# Engine source and modifications

Chenyi Office ships a Windows x64 engine based on the LibreOffice 26.8.3
source release. This file closes the MPL 2.0 source-availability requirement
for the modified engine resources. It is a legal/provenance record, not a
product-facing page.

## Upstream

- Project: LibreOffice
- Version: 26.8.3
- Upstream source: https://git.libreoffice.org/core
- Upstream release notes: https://www.libreoffice.org/download/release-notes/
- MSI used as the build input: `LibreOffice_26.8.3_Win_x86-64.msi`
- Pinned SHA-256: `0e6918c273eabbdbbd800ee0dacbbf130a14c13c2e4cec861b06dd59e015bf94`

The MSI is mirrored as `ChenyiEngine-26.8.3-x64.msi` in the v0.3.1 GitHub
release so the build no longer downloads it from an upstream mirror. The
mirrored file is byte-identical to the pinned upstream MSI.

## Files modified by this project

The installer copies these generated files into the engine tree after the MSI
completes (bundled engine only; a pre-existing engine is never modified):

- `share/config/soffice.cfg/modules/swriter/ui/notebookbar.ui`
- `share/config/soffice.cfg/modules/swriter/menubar/menubar.xml`
- `share/config/soffice.cfg/modules/scalc/ui/notebookbar.ui`
- `share/config/soffice.cfg/modules/scalc/menubar/menubar.xml`
- `share/config/soffice.cfg/modules/simpress/ui/notebookbar.ui`
- `share/config/soffice.cfg/modules/simpress/menubar/menubar.xml`
- `share/config/soffice.cfg/modules/sdraw/ui/notebookbar.ui`
- `share/config/soffice.cfg/modules/sdraw/menubar/menubar.xml`
- `share/config/soffice.cfg/sfx/ui/startcenter.ui`
- `share/config/soffice.cfg/cui/ui/aboutdialog.ui`
- `program/intro.png`, `program/intro-highres.png`
- `program/shell/logo.svg`, `program/shell/logo-sc.svg`, `program/shell/about.svg`

The generated files are committed under `app/engine-ui/` and are produced by
`scripts/make-engine-ui.py`. The script records the transformations (tab order,
Home group, menu pruning, Start Center labels, About dialog labels). The
artwork is produced by `scripts/make-engine-assets.py`.

## Source Code Form

The Source Code Form for the modified engine resources is:

1. the upstream LibreOffice source at the version above, plus
2. the generated files under `app/engine-ui/` and the generators under
   `scripts/`.

Both are available in this public repository. Files retain their original
MPL 2.0 headers and notices.
