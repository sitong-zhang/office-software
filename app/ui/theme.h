// Shared GDI+ interface palette and drawing helpers.
// Layout and token choices are documented in docs/ui-references.md.

#pragma once

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <shlwapi.h>

#include <map>
#include <string>

namespace ui {

using namespace Gdiplus;

enum IconVariant {
  ICON_DARK = 0,
  ICON_CREAM = 1,
  ICON_GREEN = 2,
  ICON_MUTED = 3,
  ICON_SOFT = 4,
};

inline Color rgba(int r, int g, int b, int a = 255) { return Color(a, r, g, b); }

inline Color Ink() { return rgba(16, 32, 26); }
inline Color InkSoft() { return rgba(27, 48, 40); }
inline Color Green() { return rgba(31, 92, 70); }
inline Color GreenLight() { return rgba(46, 125, 91); }
inline Color Accent() { return rgba(88, 184, 146); }
inline Color Paper() { return rgba(247, 245, 239); }
inline Color Surface() { return rgba(255, 255, 255); }
inline Color SurfaceMuted() { return rgba(239, 236, 227); }
inline Color Border() { return rgba(226, 221, 208); }
inline Color Text() { return rgba(23, 32, 27); }
inline Color TextMuted() { return rgba(107, 114, 105); }
inline Color TextSoft() { return rgba(156, 170, 160); }
inline Color Danger() { return rgba(184, 72, 58); }
inline Color DangerSoft() { return rgba(248, 232, 228); }
inline Color SidebarText() { return rgba(244, 241, 232); }
inline Color SidebarMuted() { return rgba(168, 184, 174); }
inline Color SidebarActive() { return rgba(38, 78, 61); }

inline int S(int value, int dpi) { return MulDiv(value, dpi, 96); }

inline void InitGdiplus(ULONG_PTR& token) {
  GdiplusStartupInput input;
  GdiplusStartup(&token, &input, nullptr);
}

inline void ShutdownGdiplus(ULONG_PTR token) {
  if (token) GdiplusShutdown(token);
}

inline void AddRoundRect(GraphicsPath& path, const RectF& r, float radius) {
  float diameter = radius * 2.0f;
  if (diameter > r.Width) diameter = r.Width;
  if (diameter > r.Height) diameter = r.Height;
  path.AddArc(r.X, r.Y, diameter, diameter, 180.0f, 90.0f);
  path.AddArc(r.X + r.Width - diameter, r.Y, diameter, diameter, 270.0f, 90.0f);
  path.AddArc(r.X + r.Width - diameter, r.Y + r.Height - diameter, diameter, diameter, 0.0f,
              90.0f);
  path.AddArc(r.X, r.Y + r.Height - diameter, diameter, diameter, 90.0f, 90.0f);
  path.CloseFigure();
}

inline void FillRound(Graphics& g, const RectF& r, float radius, const Color& color) {
  if (r.Width <= 0 || r.Height <= 0) return;
  GraphicsPath path;
  AddRoundRect(path, r, radius);
  SolidBrush brush(color);
  g.FillPath(&brush, &path);
}

inline void FillRoundGradient(Graphics& g, const RectF& r, float radius, const Color& top,
                              const Color& bottom) {
  if (r.Width <= 0 || r.Height <= 0) return;
  GraphicsPath path;
  AddRoundRect(path, r, radius);
  LinearGradientBrush brush(r, top, bottom, LinearGradientModeVertical);
  g.FillPath(&brush, &path);
}

inline void StrokeRound(Graphics& g, const RectF& r, float radius, const Color& color,
                        float width = 1.0f) {
  if (r.Width <= 0 || r.Height <= 0) return;
  GraphicsPath path;
  AddRoundRect(path, r, radius);
  Pen pen(color, width);
  g.DrawPath(&pen, &path);
}

inline void SoftShadow(Graphics& g, const RectF& r, float radius) {
  for (int i = 8; i >= 1; --i) {
    RectF shadow(r.X - i, r.Y - i + 2.0f, r.Width + i * 2, r.Height + i * 2);
    int alpha = 3 + (8 - i);
    FillRound(g, shadow, radius + i, rgba(16, 32, 26, alpha));
  }
}

inline FontFamily* PickFamily(const wchar_t* family, FontFamily& first, FontFamily& second,
                              FontFamily& third, FontFamily& fourth, FontFamily& fifth) {
  if (first.GetLastStatus() == Ok) return &first;
  if (second.GetLastStatus() == Ok) return &second;
  if (third.GetLastStatus() == Ok) return &third;
  if (fourth.GetLastStatus() == Ok) return &fourth;
  if (fifth.GetLastStatus() == Ok) return &fifth;
  return &first;
}

inline void Text(Graphics& g, const std::wstring& text, const RectF& rect, float size,
                 INT style, const Color& color, StringAlignment align = StringAlignmentNear,
                 StringAlignment line = StringAlignmentNear, bool wrap = false,
                 const wchar_t* family = L"Microsoft YaHei UI") {
  if (text.empty()) return;
  FontFamily first(family);
  FontFamily second(L"Microsoft YaHei UI");
  FontFamily third(L"Segoe UI");
  FontFamily fourth(L"Arial");
  FontFamily fifth(L"Tahoma");
  FontFamily* fontFamily = PickFamily(family, first, second, third, fourth, fifth);
  Font font(fontFamily, size, style, UnitPixel);
  SolidBrush brush(color);
  StringFormat format;
  format.SetAlignment(align);
  format.SetLineAlignment(line);
  format.SetTrimming(StringTrimmingEllipsisCharacter);
  INT flags = StringFormatFlagsNoClip;
  if (!wrap) flags |= StringFormatFlagsNoWrap;
  format.SetFormatFlags(flags);
  // ClearType is not supported on 32-bit alpha surfaces; use anti-aliasing.
  g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
  g.DrawString(text.c_str(), (INT)text.size(), &font, rect, &format, &brush);
}

inline void TextLatin(Graphics& g, const std::wstring& text, const RectF& rect, float size,
                      INT style, const Color& color,
                      StringAlignment align = StringAlignmentNear, bool wrap = false) {
  Text(g, text, rect, size, style, color, align, StringAlignmentNear, wrap, L"Segoe UI");
}

inline RectF MeasureText(Graphics& g, const std::wstring& text, const RectF& layout, float size,
                         INT style, bool wrap = true,
                         const wchar_t* family = L"Microsoft YaHei UI") {
  FontFamily first(family);
  FontFamily second(L"Microsoft YaHei UI");
  FontFamily third(L"Segoe UI");
  FontFamily fourth(L"Arial");
  FontFamily fifth(L"Tahoma");
  FontFamily* fontFamily = PickFamily(family, first, second, third, fourth, fifth);
  Font font(fontFamily, size, style, UnitPixel);
  StringFormat format;
  format.SetTrimming(StringTrimmingEllipsisCharacter);
  INT flags = StringFormatFlagsNoClip;
  if (!wrap) flags |= StringFormatFlagsNoWrap;
  format.SetFormatFlags(flags);
  RectF bounds;
  g.MeasureString(text.c_str(), (INT)text.size(), &font, layout, &format, &bounds);
  return bounds;
}

inline Bitmap* IconBitmap(int resourceId) {
  static std::map<int, Bitmap*> cache;
  auto found = cache.find(resourceId);
  if (found != cache.end()) return found->second;

  HRSRC resource = FindResourceW(nullptr, MAKEINTRESOURCEW(resourceId), RT_RCDATA);
  if (!resource) return nullptr;
  HGLOBAL loaded = LoadResource(nullptr, resource);
  DWORD size = SizeofResource(nullptr, resource);
  const void* bytes = LockResource(loaded);
  if (!bytes || size == 0) return nullptr;

  IStream* stream = SHCreateMemStream((const BYTE*)bytes, size);
  if (!stream) return nullptr;
  Bitmap* bitmap = Bitmap::FromStream(stream);
  stream->Release();
  if (!bitmap || bitmap->GetLastStatus() != Ok) {
    delete bitmap;
    return nullptr;
  }
  cache[resourceId] = bitmap;
  return bitmap;
}

inline void Image(Graphics& g, int resourceId, const RectF& rect) {
  Bitmap* bitmap = IconBitmap(resourceId);
  if (!bitmap) return;
  g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
  g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
  g.DrawImage(bitmap, rect);
}

inline void Icon(Graphics& g, int baseId, IconVariant variant, const RectF& rect) {
  Bitmap* bitmap = IconBitmap(baseId + (int)variant);
  if (!bitmap) return;
  g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
  g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
  g.DrawImage(bitmap, rect);
}

inline bool Hit(const RectF& rect, POINT point) {
  return point.x >= rect.X && point.x < rect.X + rect.Width && point.y >= rect.Y &&
         point.y < rect.Y + rect.Height;
}

inline void ApplyWindowChrome(HWND window, int width, int height, int radius, bool dark) {
  HRGN region = CreateRoundRectRgn(0, 0, width + 1, height + 1, radius * 2, radius * 2);
  if (region) SetWindowRgn(window, region, TRUE);

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
  DWORD preference = 2;  // DWMWCP_ROUND
  DwmSetWindowAttribute(window, DWMWA_WINDOW_CORNER_PREFERENCE, &preference,
                        sizeof(preference));
  BOOL darkMode = dark ? TRUE : FALSE;
  DwmSetWindowAttribute(window, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));
}

}  // namespace ui
