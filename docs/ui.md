# Interface direction

The product is rebuilt around its interface. The goal is not a new coat of paint on the upstream shell, but a consistent, quieter workspace that a first-time user can operate without instruction.

## Principles

1. **Document first.** Chrome recedes when the user types; commands appear in context rather than all at once.
2. **One visual language.** The Start Center, editor, dialogs, settings, and update screens share typography, spacing, icon style, and motion.
3. **Fewer, clearer defaults.** A curated set of templates and commands is visible; the rest stays one level away.
4. **Calm motion.** Animation communicates state — startup, loading, update — and never delays work.
5. **Windows-native behavior.** DPI scaling, high-contrast mode, keyboard navigation, and window controls follow Windows 10/11 conventions.

## Surfaces in scope

| Surface | Change |
| --- | --- |
| Splash | Branded startup animation, doubles as the update check state |
| Start Center | Simplified layout: recent documents, templates, create-new |
| Menus and toolbars | Reduced default density, consistent icon grid, grouped commands |
| Dialogs | Shared spacing, type scale, button order, and validation states |
| Settings | Reorganized by task instead of upstream module tree |
| Update screen | Full-window blocking state, see [updater.md](updater.md) |
| Announcement screen | Full-window notice shown once, dismissed with **继续** |
| About | Product name, version, attribution line as required by [branding.md](branding.md) |

## Design tokens

- **Type scale**: 12 / 13 / 15 / 18 / 24 / 32 px; UI text uses the Windows system UI font, with a CJK fallback stack.
- **Spacing**: 4 px base grid; 8, 12, 16, 24, 32.
- **Corner radius**: 6 px for controls, 10 px for dialogs.
- **Color**: neutral surfaces, a single accent used for primary actions and focus.
- **Icons**: one 24 px grid, 2 px stroke, consistent optical weight; no upstream icon set reused as product identity.

## Splash animation

The splash is the first brand impression and the first update checkpoint.

- **Duration**: 1.2–1.8 s when the app is ready; it persists while the update check runs, up to 10 s.
- **Content**: product wordmark, edition suffix, loading indicator, version in small type.
- **Motion**: wordmark fade-and-rise, a single accent line that resolves into the progress state. No looping decorative motion after the intro.
- **Behavior**: not interactive; if the update check returns "up to date", the main window opens immediately after the animation completes.
- **First launch**: one additional line of setup copy; total time capped at 2.5 s.
- **Reduced motion**: cross-fade only, 300 ms.
- **Failure**: if the update check cannot complete, the splash exits to the app with a non-blocking notice and rechecks later.

## Accessibility

- All new surfaces are keyboard reachable, with visible focus states.
- Contrast meets WCAG 2.1 AA for text and controls.
- High-contrast and dark themes are part of the token set, not an afterthought.
- Animation respects the Windows "Show animations" and reduced-motion settings.

## Deliverables before the first preview

- [ ] Token sheet and component inventory
- [ ] Start Center and main-window mockups
- [ ] Splash animation source file plus exported frames
- [ ] Update screen mockup covering checking, downloading, verifying, installing, failed
- [ ] Announcement screen mockup for one and multiple notices
- [ ] Icon set for the application, file types, and window chrome
- [ ] Accessibility and high-contrast review
