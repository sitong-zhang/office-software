# UI references and copied layout sources

The Chenyi Office shell is native Win32 C++ (GDI+). It cannot link React or
Tailwind components directly, so the layout structure, spacing scale, and
component patterns are ported from the following high-star open-source
projects. Icon assets are vendored directly.

| Project | Stars | License | What was reused |
| --- | --- | --- | --- |
| [shadcn/ui](https://github.com/shadcn-ui/ui) | 128,200 | MIT | Card, border, muted surface, radius, and typography tokens |
| [Tabler](https://github.com/tabler/tabler) | 42,064 | MIT | Sidebar + page header + card grid dashboard layout |
| [Fluent UI](https://github.com/microsoft/fluentui) | 20,407 | MIT | Windows window chrome, button, and focus conventions |
| [Lucide](https://github.com/lucide-icons/lucide) | 25,758 | ISC | Interface icons, vendored in `app/branding/third-party/lucide/` |
| [Material Design Icons](https://github.com/google/material-design-icons) | 54,299 | Apache-2.0 | Brand mark, vendored in `app/branding/third-party/material-design-icons/` |

## Layout mapping

- **Launcher**: Tabler-style vertical sidebar on the left, page header across
  the content area, hero card, 2×2 quick-create card grid, recent-documents
  card, and a right rail with version, announcement, and engine cards.
- **Installer**: Tabler-style sidebar with a three-step wizard, content header,
  feature rows, password field, and a bottom-right primary action.
- **Uninstaller**: same sidebar + content split, with a four-step cleanup list
  and a completion card.

## What is original to this project

- The native Win32/GDI+ implementation of the layout.
- The product copy, update flow, installer logic, and password gate.
- The Material `description` brand mark composition (text and placement only;
  the glyph geometry is unchanged).

## What was not copied

- No Tabler Bootstrap CSS or JavaScript is linked at runtime; only the layout
  structure and spacing conventions were ported to native code.
- Icons are used under their respective licenses, with full license texts in
  `app/branding/third-party/`.
