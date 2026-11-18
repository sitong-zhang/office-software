// Shared composite GDI controls for the three desktop executables
// (launcher, installer, uninstaller).
//
// These helpers are extracted verbatim from the three copies that used to
// live in each main.cpp. The only structural change is that per-executable
// globals (g_dpi, g_hover, the hit list) are passed in explicitly through
// DrawState / HitMap, so a control never reads another module's state.
// Pixel geometry, colours and icon variants are unchanged: replacing a copy
// with a call here is a behaviour-preserving refactor.
//
// Dependencies: theme.h (palette + primitives), icons.h (icon ids).

#pragma once

#include "app/ui/icons.h"
#include "app/ui/theme.h"

#include <vector>

namespace ui {

// ---------------------------------------------------------------------------
// Hit testing — one implementation shared by all three executables.
// ---------------------------------------------------------------------------

struct HitTarget {
  RectF rect;
  int id;
};

class HitMap {
 public:
  void Add(const RectF& rect, int id) {
    if (rect.Width > 0 && rect.Height > 0) targets.push_back({rect, id});
  }
  // Later registrations win (controls registered last are drawn on top).
  int Test(POINT point, int none) const {
    for (auto it = targets.rbegin(); it != targets.rend(); ++it)
      if (Hit(it->rect, point)) return it->id;
    return none;
  }
  void Clear() { targets.clear(); }

 private:
  std::vector<HitTarget> targets;
};

// ---------------------------------------------------------------------------
// Per-window draw state passed to every composite control.
// ---------------------------------------------------------------------------

struct DrawState {
  int dpi;
  int hover;
  int s(int value) const { return MulDiv(value, dpi, 96); }
};

// ---------------------------------------------------------------------------
// Composite controls.
// ---------------------------------------------------------------------------

// Primary / secondary action button with optional leading icon.
inline void Button(Graphics& g, const DrawState& ds, HitMap& hits, const RectF& rect,
                   const std::wstring& label, int icon, bool primary, int action,
                   bool dark = false) {
  bool hover = ds.hover == action;
  if (primary) {
    FillRound(g, rect, ds.s(10), hover ? GreenLight() : Green());
  } else {
    FillRound(g, rect, ds.s(10), hover ? SurfaceMuted() : Surface());
    StrokeRound(g, rect, ds.s(10), Border(), 1.0f);
  }
  float iconSize = (float)ds.s(17);
  float textX = rect.X + ds.s(16);
  if (icon) {
    Icon(g, icon, primary || dark ? ICON_CREAM : ICON_GREEN,
         RectF(rect.X + ds.s(14), rect.Y + (rect.Height - iconSize) / 2, iconSize, iconSize));
    textX += iconSize + ds.s(8);
  }
  Text(g, label, RectF(textX, rect.Y, rect.Width - (textX - rect.X) - ds.s(12), rect.Height),
       (float)ds.s(13), FontStyleBold, primary ? Surface() : Text(), StringAlignmentNear,
       StringAlignmentCenter);
  hits.Add(rect, action);
}

// Caption-bar minimise / close glyph button.
inline void WindowButton(Graphics& g, const DrawState& ds, HitMap& hits, const RectF& rect,
                         int icon, int action, bool dark) {
  bool hover = ds.hover == action;
  if (hover)
    FillRound(g, rect, ds.s(8), dark ? rgba(255, 255, 255, 24) : rgba(16, 32, 26, 12));
  Icon(g, icon, dark ? ICON_CREAM : ICON_MUTED,
       RectF(rect.X + ds.s(6), rect.Y + ds.s(6), ds.s(16), ds.s(16)));
  hits.Add(rect, action);
}

// Standard card surface: optional soft shadow, white fill, thin border.
// `radius` is already DPI-scaled by the caller.
inline void Card(Graphics& g, const RectF& rect, float radius, bool shadow = true) {
  if (shadow) SoftShadow(g, rect, radius);
  FillRound(g, rect, radius, Surface());
  StrokeRound(g, rect, radius, Border(), 1.0f);
}

// Horizontal progress track with a green filled portion.
inline void ProgressBar(Graphics& g, const DrawState& ds, const RectF& rect, int percent) {
  FillRound(g, rect, ds.s(5), SurfaceMuted());
  if (percent > 0)
    FillRound(g, RectF(rect.X, rect.Y, rect.Width * percent / 100.0f, rect.Height), ds.s(5),
              Green());
}

// Shared title bar: title + subtitle + divider + minimise/close controls.
// `sidebar` and `width` are already DPI-scaled screen coordinates.
inline void TitleBar(Graphics& g, const DrawState& ds, HitMap& hits, float sidebar, float width,
                     const std::wstring& title, const std::wstring& subtitle,
                     int minimizeAction, int closeAction, bool allowClose) {
  Text(g, title, RectF(sidebar + ds.s(36), ds.s(12), ds.s(480), ds.s(32)), (float)ds.s(17),
       FontStyleBold, Text());
  Text(g, subtitle, RectF(sidebar + ds.s(36), ds.s(34), ds.s(480), ds.s(16)), (float)ds.s(10),
       FontStyleRegular, TextMuted());
  FillRound(g, RectF(sidebar, ds.s(56) - 1, width - sidebar, 1), 0, Border());
  WindowButton(g, ds, hits, RectF(width - ds.s(86), ds.s(14), ds.s(28), ds.s(28)),
               UI_ICON_MINIMIZE, minimizeAction, false);
  if (allowClose)
    WindowButton(g, ds, hits, RectF(width - ds.s(52), ds.s(14), ds.s(28), ds.s(28)),
                 UI_ICON_CLOSE, closeAction, false);
}

}  // namespace ui
