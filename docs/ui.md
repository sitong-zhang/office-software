# Interface direction

The product is rebuilt around its interface: a consistent, quieter workspace that a first-time user can operate without instruction.

The shell layout is ported from high-star open-source projects; see
[ui-references.md](ui-references.md) for the exact sources, licenses, and
mapping.

## Implemented in v0.3.1

- **Tabbed ribbon** is the default UI for Writer, Calc, Impress, and Draw, with a WPS-style tab order (文件 / 开始 / 插入 / 页面布局 / 引用 / 审阅 / 视图 / 工具 / 扩展).
- **编辑 group** on the Home tab of all four applications, with Find & Replace and Select All.
- **Sifr icon theme** for a quieter, modern look.
- **Engine artwork**: custom splash screens (`intro.png`, `intro-highres.png`) and Start Center / About marks (`logo.svg`, `logo-sc.svg`, `about.svg`) are written into the installed engine.
- **Product identity** inside the engine is set to Chenyi Office (name and vendor).
- **Clean first run**: tip of the day, What's New, feedback, and getting-involved entries are disabled or removed. The editor Help menus keep help, safe mode, command search, and the licence entry.
- **Start Center** no longer shows the promotional panel or button.
- **Isolated profile**: the launcher starts the engine with `-env:UserInstallation=%LOCALAPPDATA%\ChenyiOffice\lo-profile`, so the branded configuration does not collide with other installations.
- **Brand shell**: splash animation, password-gated installer, update screen, and announcement screen.

## Next: deeper interface work

The following require rebuilding the engine with the platform toolchain:

- Dialog and settings reorganization.
- Consistent typography, spacing, and color tokens across controls.
- Dark theme, high contrast, and per-monitor DPI review.
- Engine-level icons, default templates, and individual dialog layouts.

## Principles

1. **Document first.** Chrome recedes when the user types; commands appear in context rather than all at once.
2. **One visual language.** The Start Center, editor, dialogs, settings, and update screens share typography, spacing, icon style, and motion.
3. **Fewer, clearer defaults.** A curated set of templates and commands is visible; the rest stays one level away.
4. **Calm motion.** Animation communicates state — startup, loading, update — and never delays work.
5. **Windows-native behavior.** DPI scaling, high-contrast mode, keyboard navigation, and window controls follow Windows 10/11 conventions.

## Surfaces in scope

| Surface | Change |
| --- | --- |
| Installer welcome | Branded background, logo, introduction, password gate, **下一步确认** |
| Splash | Branded startup animation, doubles as the update check state |
| Start Center | Simplified layout: recent documents, templates, create-new, no promotional panel |
| Ribbon and menus | Reordered tabs, compact Home groups, promotional entries removed |
| Dialogs | Shared spacing, type scale, button order, and validation states |
| Settings | Reorganized by task instead of the engine's module tree |
| Update screen | Full-window blocking state, see [updater.md](updater.md) |
| Announcement screen | Full-window notice shown once, dismissed with **继续** |
| About | Product name, edition, and platform |

## Design tokens

- **Type scale**: 12 / 13 / 15 / 18 / 24 / 32 px; UI text uses the Windows system UI font, with a CJK fallback stack.
- **Spacing**: 4 px base grid; 8, 12, 16, 24, 32.
- **Corner radius**: 6 px for controls, 10 px for dialogs.
- **Color**: neutral surfaces, a single accent used for primary actions and focus.
- **Icons**: one 24 px grid, 2 px stroke, consistent optical weight; no vendor icon set reused as product identity.

## Splash animation

The splash is the first brand impression and the first update checkpoint. Implemented in `app/launcher/main.cpp`.

- **Duration**: 1.2–1.8 s when the app is ready; it persists while the update check runs, up to 10 s.
- **Content**: product wordmark, edition suffix, loading indicator, version in small type.
- **Motion**: wordmark fade-and-rise, a single accent line that resolves into the progress state. No looping decorative motion after the intro.
- **Behavior**: not interactive; if the update check returns "up to date", the main window opens immediately after the animation completes.
- **First launch**: one additional line of setup copy; total time capped at 2.5 s.
- **Reduced motion**: cross-fade only, 300 ms.
- **Failure**: if the update check cannot complete, the splash exits to the app with a non-blocking notice; the next launch retries.

## Accessibility

- All new surfaces are keyboard reachable, with visible focus states.
- Contrast meets WCAG 2.1 AA for text and controls.
- High-contrast and dark themes are part of the token set, not an afterthought.
- Animation respects the Windows "Show animations" and reduced-motion settings.

Current limitations: the launcher, installer, and uninstaller use custom-drawn GDI surfaces with fixed layouts. Enter activates the primary action and the installer password field takes focus, but there is no full Tab focus chain, no UI Automation semantics, and no high-contrast theme yet. A full accessibility review remains planned.

## Deliverables before the next preview

- [ ] Token sheet and component inventory
- [ ] Dialog and settings layouts
- [ ] High-contrast review
- [ ] Accessibility and keyboard-navigation review
