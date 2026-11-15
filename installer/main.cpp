// Chenyi Office（胡辰逸定制版） installer
// Welcome gate with password, payload extraction, engine installation.

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define _WIN32_WINNT 0x0A00

#include <windows.h>
#include <windowsx.h>
#include <bcrypt.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <objidl.h>

#include "app/ui/icons.h"
#include "app/ui/theme.h"

using namespace Gdiplus;

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#if __has_include("product_code.h")
#include "product_code.h"
#endif
#ifndef MSI_PRODUCT_CODE
#define MSI_PRODUCT_CODE L""
#endif

static const wchar_t* kProduct = L"Chenyi Office（胡辰逸定制版）";
static const wchar_t* kVersion = L"0.4.1";
// SHA-256 of the install password; the plain password is not stored in source.
// This is a distribution gate, not a security boundary: the installer payload
// can always be extracted by someone who obtains the file.
static const wchar_t* kPasswordHash =
    L"d19e5dbfd3d3afe04f12e4f5c12085065e973246970d42608186ab4d20d4a9c5";

// The bundled engine payload ships with a self-contained identity.

enum Screen { WELCOME = 0, INSTALLING = 1, FINISH = 2, FAILED = 3 };

static HINSTANCE g_instance = nullptr;
static HWND g_window = nullptr;
static HWND g_edit = nullptr;
static WNDPROC g_editProc = nullptr;
static HFONT g_editFont = nullptr;
static HBITMAP g_background = nullptr;
static HICON g_icon = nullptr;
static int g_dpi = 96;
static int g_width = 980;
static int g_height = 680;
static const int kBaseWidth = 980;
static const int kBaseHeight = 680;
static ULONG_PTR g_gdiplusToken = 0;
static int g_hover = 0;
static Screen g_screen = WELCOME;
static bool g_passwordError = false;
static bool g_enginePreExisting = false;
static bool g_updateMode = false;
static std::wstring g_error;
static std::wstring g_installDir;

static volatile LONG g_stage = 0;
static volatile LONG g_state = 0;  // 0 idle, 1 working, 2 done, 3 failed
static volatile LONG g_extractPercent = 0;
static volatile LONG g_marquee = 0;

#define S(x) MulDiv((x), g_dpi, 96)
#define SIDEBAR S(280)
#define TITLEBAR S(56)

enum InstallerAction {
  IA_NONE = 0,
  IA_START,
  IA_RETRY,
  IA_OPEN,
  IA_EXIT,
  IA_MINIMIZE,
  IA_CLOSE,
};

struct InstallerHit {
  RectF rect;
  int id;
};

static std::vector<InstallerHit> g_hits;

static void AddHit(const RectF& rect, int id) {
  if (rect.Width > 0 && rect.Height > 0) g_hits.push_back({rect, id});
}

static int HitTest(POINT point) {
  for (auto it = g_hits.rbegin(); it != g_hits.rend(); ++it) {
    if (ui::Hit(it->rect, point)) return it->id;
  }
  return IA_NONE;
}

// ---------------------------------------------------------------------------
// Drawing helpers
// ---------------------------------------------------------------------------

static void DrawButton(Graphics& g, const RectF& rect, const std::wstring& label, int icon,
                       bool primary, int action) {
  bool hover = g_hover == action;
  if (primary) {
    ui::FillRound(g, rect, S(10), hover ? ui::GreenLight() : ui::Green());
  } else {
    ui::FillRound(g, rect, S(10), hover ? ui::SurfaceMuted() : ui::Surface());
    ui::StrokeRound(g, rect, S(10), ui::Border(), 1.0f);
  }
  float iconSize = S(17);
  float textX = rect.X + S(16);
  if (icon) {
    ui::Icon(g, icon, primary ? ui::ICON_CREAM : ui::ICON_GREEN,
             RectF(rect.X + S(14), rect.Y + (rect.Height - iconSize) / 2, iconSize, iconSize));
    textX += iconSize + S(8);
  }
  ui::Text(g, label, RectF(textX, rect.Y, rect.Width - (textX - rect.X) - S(12), rect.Height),
           S(13), FontStyleBold, primary ? ui::Surface() : ui::Text(), StringAlignmentNear,
           StringAlignmentCenter);
  AddHit(rect, action);
}

static void DrawWindowButton(Graphics& g, const RectF& rect, int icon, int action, bool dark) {
  bool hover = g_hover == action;
  if (hover) {
    ui::FillRound(g, rect, S(8), dark ? ui::rgba(255, 255, 255, 24) : ui::rgba(16, 32, 26, 12));
  }
  ui::Icon(g, icon, dark ? ui::ICON_CREAM : ui::ICON_MUTED,
           RectF(rect.X + S(6), rect.Y + S(6), S(16), S(16)));
  AddHit(rect, action);
}

static void DrawStep(Graphics& g, int y, int icon, const std::wstring& label, bool active,
                     bool done) {
  RectF dot(S(28), (float)y, S(32), S(32));
  ui::FillRound(g, dot, S(10), active ? ui::SidebarActive()
                                      : (done ? ui::rgba(88, 184, 146, 40)
                                              : ui::rgba(255, 255, 255, 10)));
  ui::Icon(g, done ? UI_ICON_CHECK_CIRCLE : icon,
           active || done ? ui::ICON_CREAM : ui::ICON_SOFT,
           RectF(dot.X + S(8), dot.Y + S(8), S(16), S(16)));
  ui::Text(g, label, RectF(S(74), y, S(180), S(32)), S(12),
           active ? FontStyleBold : FontStyleRegular,
           active || done ? ui::SidebarText() : ui::SidebarMuted(), StringAlignmentNear,
           StringAlignmentCenter);
}

static void DrawSidebar(Graphics& g) {
  RectF side(0, 0, SIDEBAR, g_height);
  LinearGradientBrush background(side, ui::rgba(22, 50, 39), ui::rgba(10, 24, 19),
                                 LinearGradientModeVertical);
  g.FillRectangle(&background, side);

  ui::Image(g, 111, RectF(S(26), S(24), S(42), S(42)));
  ui::TextLatin(g, L"Chenyi Office", RectF(S(80), S(22), S(170), S(24)), S(15),
                FontStyleBold, ui::SidebarText());
  ui::Text(g, L"胡辰逸定制版", RectF(S(80), S(48), S(170), S(18)), S(10),
           FontStyleRegular, ui::SidebarMuted());
  ui::FillRound(g, RectF(S(26), S(88), S(228), 1), 0, ui::rgba(255, 255, 255, 18));

  int y = S(140);
  DrawStep(g, y, UI_ICON_SHIELD_CHECK, L"欢迎", g_screen == WELCOME, g_screen != WELCOME);
  y += S(58);
  DrawStep(g, y, UI_ICON_DOWNLOAD, L"安装", g_screen == INSTALLING,
           g_screen == FINISH);
  y += S(58);
  DrawStep(g, y, UI_ICON_CHECK_CIRCLE, L"完成", g_screen == FINISH, false);

  ui::Text(g, L"安装向导", RectF(S(26), S(400), S(220), S(20)), S(9.5), FontStyleBold,
           ui::rgba(136, 196, 168));
  ui::Text(g, L"大约需要几分钟。安装期间请保持窗口打开。",
           RectF(S(26), S(426), S(220), S(44)), S(9.5), FontStyleRegular, ui::SidebarMuted(),
           StringAlignmentNear, StringAlignmentNear, true);

  ui::TextLatin(g, std::wstring(L"v") + kVersion, RectF(S(26), g_height - S(62), S(120), S(20)),
                S(11), FontStyleBold, ui::SidebarText());
  ui::Text(g, L"Windows 10 / 11 · x64", RectF(S(26), g_height - S(38), S(228), S(18)), S(9),
           FontStyleRegular, ui::SidebarMuted());
}

static void DrawTitleBar(Graphics& g, const std::wstring& title, bool allowClose) {
  ui::Text(g, title, RectF(SIDEBAR + S(36), S(12), S(480), S(32)), S(17), FontStyleBold,
           ui::Text());
  ui::Text(g, L"Chenyi Office · 胡辰逸定制版", RectF(SIDEBAR + S(36), S(34), S(480), S(16)),
           S(10), FontStyleRegular, ui::TextMuted());
  ui::FillRound(g, RectF(SIDEBAR, TITLEBAR - 1, g_width - SIDEBAR, 1), 0, ui::Border());
  DrawWindowButton(g, RectF(g_width - S(86), S(14), S(28), S(28)), UI_ICON_MINIMIZE, IA_MINIMIZE,
                   false);
  if (allowClose) {
    DrawWindowButton(g, RectF(g_width - S(52), S(14), S(28), S(28)), UI_ICON_CLOSE, IA_CLOSE,
                     false);
  }
}

static void DrawWelcome(Graphics& g) {
  DrawTitleBar(g, L"安装 Chenyi Office", true);
  float x = S(320), width = S(600);
  ui::Text(g, L"WINDOWS 10 / 11 · 64 位", RectF(x, S(88), width, S(18)), S(9.5),
           FontStyleBold, ui::Green());
  ui::Text(g, L"安装 Chenyi Office", RectF(x, S(110), width, S(42)), S(28), FontStyleBold,
           ui::Text());
  ui::Text(g,
           L"一套以界面为先的办公套件：文档、表格、演示与绘图都在同一个安静的工作空间里完成。",
           RectF(x, S(156), width, S(42)), S(11.5), FontStyleRegular, ui::TextMuted(),
           StringAlignmentNear, StringAlignmentNear, true);

  struct Feature {
    int icon;
    const wchar_t* title;
    const wchar_t* text;
  };
  Feature features[] = {
      {UI_ICON_SHIELD_CHECK, L"安全更新", L"安装包校验与发布者签名验证。"},
      {UI_ICON_FILE_TEXT, L"完整办公套件", L"Writer、Calc、Impress 与 Draw。"},
      {UI_ICON_SPARKLES, L"品牌工作区", L"Tabbed 标签页、Sifr 图标与专属开屏。"},
  };
  float y = S(216);
  for (const auto& feature : features) {
    RectF tile(x, y, S(38), S(38));
    ui::FillRound(g, tile, S(11), ui::SurfaceMuted());
    ui::Icon(g, feature.icon, ui::ICON_GREEN, RectF(tile.X + S(8), tile.Y + S(8), S(22), S(22)));
    ui::Text(g, feature.title, RectF(x + S(52), y - S(2), width - S(52), S(20)), S(12),
             FontStyleBold, ui::Text());
    ui::Text(g, feature.text, RectF(x + S(52), y + S(18), width - S(52), S(18)), S(10),
             FontStyleRegular, ui::TextMuted());
    y += S(48);
  }

  if (g_updateMode) {
    RectF note(x, S(378), width, S(76));
    ui::FillRound(g, note, S(12), ui::rgba(232, 244, 238));
    ui::Icon(g, UI_ICON_REFRESH, ui::ICON_GREEN,
             RectF(note.X + S(16), note.Y + S(24), S(24), S(24)));
    ui::Text(g, L"检测到已安装版本，本次为覆盖更新。", RectF(note.X + S(52), note.Y + S(14),
                                                        note.Width - S(68), S(22)),
             S(12), FontStyleBold, ui::Green());
    ui::Text(g, L"无需密码，直接点击右下角开始。", RectF(note.X + S(52), note.Y + S(38),
                                                     note.Width - S(68), S(20)),
             S(10), FontStyleRegular, ui::TextMuted());
  } else {
    ui::Text(g, L"安装密码", RectF(x, S(378), S(300), S(20)), S(11.5), FontStyleBold,
             ui::Text());
    ui::Text(g, L"请输入发布者提供的安装密码。", RectF(x + S(78), S(380), S(360), S(18)), S(9.5),
             FontStyleRegular, ui::TextMuted());
    if (g_passwordError) {
      ui::Text(g, L"密码错误，请重新输入。", RectF(x, S(456), width, S(18)), S(9.5),
               FontStyleRegular, ui::Danger());
    } else {
      ui::Text(g, L"密码只用于首次安装，后续更新不再询问。", RectF(x, S(456), width, S(18)),
               S(9.5), FontStyleRegular, ui::TextMuted());
    }
  }

  if (g_enginePreExisting && !g_updateMode) {
    ui::Text(g, L"检测到系统已有办公内核：将直接复用，卸载时不会移除它。",
             RectF(x, S(492), width, S(18)), S(9.5), FontStyleRegular, ui::rgba(150, 96, 44));
  }
  ui::Text(g, L"安装位置：C:\\Program Files\\ChenyiOffice", RectF(x, S(522), width, S(18)),
           S(9.5), FontStyleRegular, ui::TextMuted());
  ui::Text(g, L"Chenyi Office · 独立打造 · Windows 10 / 11",
           RectF(x, S(548), width, S(18)), S(9), FontStyleRegular, ui::TextSoft());

  DrawButton(g, RectF(g_width - S(216), S(590), S(180), S(42)),
             g_updateMode ? L"立即更新" : L"开始安装", UI_ICON_DOWNLOAD, true, IA_START);
}

static void DrawInstalling(Graphics& g) {
  DrawTitleBar(g, L"正在安装 Chenyi Office", false);
  float x = S(320), width = S(600);
  ui::Text(g, L"WINDOWS 10 / 11 · 64 位", RectF(x, S(88), width, S(18)), S(9.5),
           FontStyleBold, ui::Green());
  ui::Text(g, L"正在安装 Chenyi Office", RectF(x, S(110), width, S(42)), S(26), FontStyleBold,
           ui::Text());
  ui::Text(g, L"安装期间请保持窗口打开，完成后可以立即进入办公套件。",
           RectF(x, S(154), width, S(22)), S(11), FontStyleRegular, ui::TextMuted());

  const wchar_t* stage = L"正在准备…";
  switch (InterlockedCompareExchange(&g_stage, 0, 0)) {
    case 1: stage = L"正在解压办公内核…"; break;
    case 2: stage = L"正在安装办公内核，请稍候…"; break;
    case 3: stage = L"正在写入程序文件…"; break;
    case 4: stage = L"正在创建快捷方式…"; break;
    case 5: stage = L"正在完成安装…"; break;
    default: break;
  }
  int stageValue = (int)InterlockedCompareExchange(&g_stage, 0, 0);
  int percent = (int)InterlockedCompareExchange(&g_extractPercent, 0, 0);

  float progress = stageValue >= 5 ? 100.0f : (stageValue >= 2 ? 30.0f : percent * 0.3f);
  wchar_t pct[16];
  swprintf(pct, 16, L"%d%%", (int)progress);
  ui::TextLatin(g, pct, RectF(x, S(196), width, S(64)), S(44), FontStyleBold, ui::Green());
  ui::Text(g, stage, RectF(x + S(150), S(218), width - S(150), S(24)), S(11.5),
           FontStyleRegular, ui::TextMuted());

  RectF track(x, S(272), width, S(10));
  ui::FillRound(g, track, S(5), ui::SurfaceMuted());
  if (progress > 0) {
    ui::FillRound(g, RectF(track.X, track.Y, track.Width * progress / 100.0f, track.Height), S(5),
                  ui::Green());
  }

  const wchar_t* labels[] = {L"解压办公内核", L"安装办公内核", L"写入程序文件",
                             L"创建快捷方式", L"完成安装"};
  float y = S(320);
  for (int i = 0; i < 5; ++i) {
    bool done = stageValue > i + 1;
    bool active = stageValue == i + 1;
    RectF row(x, y, width, S(36));
    ui::FillRound(g, row, S(10), active ? ui::rgba(232, 244, 238) : ui::rgba(0, 0, 0, 0));
    ui::Icon(g, done ? UI_ICON_CHECK_CIRCLE : UI_ICON_CLOCK,
             done ? ui::ICON_GREEN : (active ? ui::ICON_GREEN : ui::ICON_SOFT),
             RectF(x + S(12), y + S(10), S(16), S(16)));
    ui::Text(g, labels[i], RectF(x + S(38), y, width - S(38), S(36)), S(11),
             active ? FontStyleBold : FontStyleRegular,
             active ? ui::Text() : ui::TextMuted(), StringAlignmentNear, StringAlignmentCenter);
    y += S(40);
  }

  RectF note(x, S(548), width, S(54));
  ui::FillRound(g, note, S(12), ui::SurfaceMuted());
  ui::Icon(g, UI_ICON_INFO, ui::ICON_GREEN, RectF(note.X + S(14), note.Y + S(17), S(20), S(20)));
  ui::Text(g, L"办公内核体积较大，首次安装通常需要几分钟。请勿关闭窗口。",
           RectF(note.X + S(44), note.Y + S(12), note.Width - S(58), S(30)), S(10),
           FontStyleRegular, ui::TextMuted(), StringAlignmentNear, StringAlignmentNear, true);
}

static void DrawFinish(Graphics& g) {
  DrawTitleBar(g, L"安装完成", true);
  float x = S(320), width = S(600);
  ui::FillRound(g, RectF(x, S(96), S(64), S(64)), S(18), ui::rgba(232, 244, 238));
  ui::Icon(g, UI_ICON_CHECK_CIRCLE, ui::ICON_GREEN, RectF(x + S(16), S(112), S(32), S(32)));
  ui::Text(g, L"安装完成", RectF(x + S(84), S(104), width - S(84), S(40)), S(26),
           FontStyleBold, ui::Text());
  ui::Text(g, L"Chenyi Office（胡辰逸定制版）已安装到本机。",
           RectF(x, S(184), width, S(22)), S(12), FontStyleBold, ui::Text());
  ui::Text(g,
           L"首次启动会执行开屏动画、检查最新版本并读取应用内公告，完成后进入办公套件。",
           RectF(x, S(214), width, S(44)), S(11), FontStyleRegular, ui::TextMuted(),
           StringAlignmentNear, StringAlignmentNear, true);
  ui::Text(g, L"安装位置", RectF(x, S(280), width, S(18)), S(10), FontStyleBold, ui::Text());
  ui::Text(g, g_installDir, RectF(x, S(302), width, S(20)), S(9.5), FontStyleRegular,
           ui::TextMuted());

  DrawButton(g, RectF(g_width - S(396), S(590), S(160), S(42)), L"打开应用", UI_ICON_ARROW_RIGHT,
             true, IA_OPEN);
  DrawButton(g, RectF(g_width - S(216), S(590), S(180), S(42)), L"退出", UI_ICON_CLOSE, false,
             IA_EXIT);
}

static void DrawFailed(Graphics& g) {
  DrawTitleBar(g, L"安装失败", true);
  float x = S(320), width = S(600);
  ui::FillRound(g, RectF(x, S(96), S(64), S(64)), S(18), ui::DangerSoft());
  ui::Icon(g, UI_ICON_INFO, ui::ICON_MUTED, RectF(x + S(16), S(112), S(32), S(32)));
  ui::Text(g, L"安装失败", RectF(x + S(84), S(104), width - S(84), S(40)), S(26),
           FontStyleBold, ui::Danger());
  ui::Text(g, g_error, RectF(x, S(188), width, S(140)), S(11.5), FontStyleRegular, ui::Text(),
           StringAlignmentNear, StringAlignmentNear, true);
  ui::Text(g, L"可以修正问题后重试；已安装的部分会被重新配置。",
           RectF(x, S(344), width, S(22)), S(10), FontStyleRegular, ui::TextMuted());
  DrawButton(g, RectF(g_width - S(216), S(590), S(180), S(42)), L"重试", UI_ICON_REFRESH, true,
             IA_RETRY);
}

static void DrawScreen(Graphics& g) {
  g.Clear(ui::Paper());
  DrawSidebar(g);
  switch (g_screen) {
    case WELCOME: DrawWelcome(g); break;
    case INSTALLING: DrawInstalling(g); break;
    case FINISH: DrawFinish(g); break;
    case FAILED: DrawFailed(g); break;
  }
}

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------

static std::wstring SelfPath() {
  wchar_t path[MAX_PATH] = {0};
  GetModuleFileNameW(nullptr, path, MAX_PATH);
  return path;
}

static std::string WideToUtf8(const std::wstring& input) {
  int size = WideCharToMultiByte(CP_UTF8, 0, input.c_str(), (int)input.size(), nullptr, 0, nullptr,
                                 nullptr);
  std::string bytes((size_t)size, '\0');
  if (size > 0) {
    WideCharToMultiByte(CP_UTF8, 0, input.c_str(), (int)input.size(), bytes.data(), size, nullptr,
                        nullptr);
  }
  return bytes;
}

static bool PasswordMatches(const std::wstring& input) {
  std::string bytes;
  int size = WideCharToMultiByte(CP_UTF8, 0, input.c_str(), (int)input.size(), nullptr, 0, nullptr,
                                 nullptr);
  bytes.resize((size_t)size);
  WideCharToMultiByte(CP_UTF8, 0, input.c_str(), (int)input.size(), &bytes[0], size, nullptr,
                      nullptr);

  BCRYPT_ALG_HANDLE alg = nullptr;
  BCRYPT_HASH_HANDLE hash = nullptr;
  DWORD objectSize = 0, result = 0;
  std::vector<BYTE> object, digest(32);
  bool ok = false;
  if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return false;
  if (BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&objectSize, sizeof(objectSize),
                        &result, 0) < 0)
    goto cleanup;
  object.resize(objectSize);
  if (BCryptCreateHash(alg, &hash, object.data(), objectSize, nullptr, 0, 0) < 0) goto cleanup;
  if (BCryptHashData(hash, (PUCHAR)bytes.data(), (ULONG)bytes.size(), 0) < 0) goto cleanup;
  if (BCryptFinishHash(hash, digest.data(), (ULONG)digest.size(), 0) < 0) goto cleanup;
  {
    static const wchar_t* digits = L"0123456789abcdef";
    std::wstring hex;
    for (BYTE b : digest) {
      hex.push_back(digits[b >> 4]);
      hex.push_back(digits[b & 0xF]);
    }
    ok = _wcsicmp(hex.c_str(), kPasswordHash) == 0;
  }

cleanup:
  if (hash) BCryptDestroyHash(hash);
  if (alg) BCryptCloseAlgorithmProvider(alg, 0);
  return ok;
}

static std::wstring TempDirectory() {
  wchar_t temp[MAX_PATH] = {0};
  GetTempPathW(MAX_PATH, temp);
  std::wstring dir = std::wstring(temp) + L"ChenyiOfficeSetup";
  SHCreateDirectoryExW(nullptr, dir.c_str(), nullptr);
  return dir;
}

static bool WriteResourceFile(int id, const std::wstring& path) {
  HRSRC res = FindResourceW(g_instance, MAKEINTRESOURCEW(id), RT_RCDATA);
  if (!res) return false;
  HGLOBAL data = LoadResource(g_instance, res);
  DWORD size = SizeofResource(g_instance, res);
  void* bytes = LockResource(data);
  if (!bytes) return false;
  HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) return false;
  DWORD written = 0;
  bool ok = WriteFile(file, bytes, size, &written, nullptr) && written == size;
  CloseHandle(file);
  return ok;
}

static bool RunAndWait(const std::wstring& command, DWORD& exitCode) {
  STARTUPINFOW si{};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi{};
  std::vector<wchar_t> buffer(command.begin(), command.end());
  buffer.push_back(0);
  if (!CreateProcessW(nullptr, buffer.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                      nullptr, &si, &pi))
    return false;
  WaitForSingleObject(pi.hProcess, INFINITE);
  GetExitCodeProcess(pi.hProcess, &exitCode);
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  return true;
}

static bool CreateShortcut(const std::wstring& linkPath, const std::wstring& target,
                           const std::wstring& description) {
  IShellLinkW* link = nullptr;
  if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW,
                              (void**)&link)))
    return false;
  link->SetPath(target.c_str());
  link->SetDescription(description.c_str());
  std::wstring dir = target.substr(0, target.find_last_of(L'\\'));
  link->SetWorkingDirectory(dir.c_str());
  link->SetIconLocation(target.c_str(), 0);

  IPersistFile* file = nullptr;
  bool ok = false;
  if (SUCCEEDED(link->QueryInterface(IID_IPersistFile, (void**)&file))) {
    ok = SUCCEEDED(file->Save(linkPath.c_str(), TRUE));
    file->Release();
  }
  link->Release();
  return ok;
}

static void SetString(HKEY root, const wchar_t* subkey, const wchar_t* name,
                      const std::wstring& value) {
  HKEY key = nullptr;
  if (RegCreateKeyExW(root, subkey, 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr) ==
      ERROR_SUCCESS) {
    RegSetValueExW(key, name, 0, REG_SZ, (const BYTE*)value.c_str(),
                   (DWORD)((value.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
  }
}

static void SetDword(HKEY root, const wchar_t* subkey, const wchar_t* name, DWORD value) {
  HKEY key = nullptr;
  if (RegCreateKeyExW(root, subkey, 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr) ==
      ERROR_SUCCESS) {
    RegSetValueExW(key, name, 0, REG_DWORD, (const BYTE*)&value, sizeof(value));
    RegCloseKey(key);
  }
}

static std::wstring GetString(HKEY root, const wchar_t* subkey, const wchar_t* name) {
  HKEY key = nullptr;
  std::wstring value;
  if (RegOpenKeyExW(root, subkey, 0, KEY_READ, &key) == ERROR_SUCCESS) {
    wchar_t buffer[1024] = {0};
    DWORD size = sizeof(buffer) - sizeof(wchar_t);
    DWORD type = 0;
    if (RegQueryValueExW(key, name, nullptr, &type, (LPBYTE)buffer, &size) == ERROR_SUCCESS &&
        type == REG_SZ) {
      value = buffer;
    }
    RegCloseKey(key);
  }
  return value;
}

static DWORD GetDword(HKEY root, const wchar_t* subkey, const wchar_t* name, DWORD fallback = 0) {
  HKEY key = nullptr;
  DWORD value = fallback;
  if (RegOpenKeyExW(root, subkey, 0, KEY_READ, &key) == ERROR_SUCCESS) {
    DWORD size = sizeof(value);
    DWORD type = 0;
    if (RegQueryValueExW(key, name, nullptr, &type, (LPBYTE)&value, &size) != ERROR_SUCCESS ||
        type != REG_DWORD) {
      value = fallback;
    }
    RegCloseKey(key);
  }
  return value;
}

// ---------------------------------------------------------------------------
// Installation worker
// ---------------------------------------------------------------------------

#pragma pack(push, 1)
struct PayloadFooter {
  char magic[8];
  uint64_t offset;
  uint64_t size;
};
#pragma pack(pop)

static void ApplyEngineBranding(const std::wstring& engineRoot) {
  if (engineRoot.empty()) return;
  std::wstring program = engineRoot + L"\\program";
  std::wstring shell = program + L"\\shell";
  if (GetFileAttributesW(program.c_str()) == INVALID_FILE_ATTRIBUTES) return;

  WriteResourceFile(150, program + L"\\intro.png");
  WriteResourceFile(151, program + L"\\intro-highres.png");
  WriteResourceFile(152, shell + L"\\logo.svg");
  WriteResourceFile(153, shell + L"\\logo-sc.svg");
  WriteResourceFile(154, shell + L"\\about.svg");

  // Deep UI layout: tab order, Home groups, menus and Start Center.
  std::wstring cfg = engineRoot + L"\\share\\config\\soffice.cfg";
  WriteResourceFile(160, cfg + L"\\modules\\swriter\\ui\\notebookbar.ui");
  WriteResourceFile(161, cfg + L"\\modules\\swriter\\menubar\\menubar.xml");
  WriteResourceFile(162, cfg + L"\\modules\\scalc\\ui\\notebookbar.ui");
  WriteResourceFile(163, cfg + L"\\modules\\scalc\\menubar\\menubar.xml");
  WriteResourceFile(164, cfg + L"\\modules\\simpress\\ui\\notebookbar.ui");
  WriteResourceFile(165, cfg + L"\\modules\\simpress\\menubar\\menubar.xml");
  WriteResourceFile(166, cfg + L"\\modules\\sdraw\\ui\\notebookbar.ui");
  WriteResourceFile(167, cfg + L"\\modules\\sdraw\\menubar\\menubar.xml");
  WriteResourceFile(168, cfg + L"\\sfx\\ui\\startcenter.ui");
  WriteResourceFile(169, cfg + L"\\cui\\ui\\aboutdialog.ui");
  WriteResourceFile(170, cfg + L"\\modules\\smath\\menubar\\menubar.xml");

  // The engine launcher resolves its kernel and bootstrap configuration from
  // its own file name, so a branded launcher needs matching .bin and .ini
  // files. Keep the originals for the engine's own internal invocations.
  CopyFileW((program + L"\\soffice.exe").c_str(),
            (program + L"\\ChenyiEngine.exe").c_str(), FALSE);
  CopyFileW((program + L"\\soffice.bin").c_str(),
            (program + L"\\ChenyiEngine.bin").c_str(), FALSE);
  CopyFileW((program + L"\\soffice.ini").c_str(),
            (program + L"\\ChenyiEngine.ini").c_str(), FALSE);
}

static void Fail(const std::wstring& message) {
  g_error = message;
  InterlockedExchange(&g_state, 3);
}

static bool EngineAlreadyInstalled() {
  std::wstring dir = GetString(HKEY_LOCAL_MACHINE, L"Software\\ChenyiOffice", L"InstallDir");
  if (dir.empty()) return false;
  // A stale registry value alone must not turn a fresh install into a
  // passwordless update.
  return GetFileAttributesW((dir + L"\\ChenyiOffice.exe").c_str()) != INVALID_FILE_ATTRIBUTES;
}

static bool PathExists(const std::wstring& path) {
  return !path.empty() && GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

static std::wstring EngineRootFromExe(const std::wstring& exe) {
  size_t program = exe.find_last_of(L'\\');
  if (program == std::wstring::npos) return L"";
  std::wstring programDir = exe.substr(0, program);
  size_t root = programDir.find_last_of(L'\\');
  if (root == std::wstring::npos) return L"";
  return programDir.substr(0, root);
}

static std::wstring FreshEngineRoot() {
  wchar_t programFiles[MAX_PATH] = {0};
  GetEnvironmentVariableW(L"ProgramFiles", programFiles, MAX_PATH);
  if (!programFiles[0]) return L"";
  return std::wstring(programFiles) + L"\\ChenyiOffice\\Engine";
}

// Returns the engine root for an existing installation, preferring the
// registry path, then the new ChenyiOffice\Engine layout, then the legacy
// layout used by earlier preview builds.
static std::wstring CurrentEngineRoot() {
  std::wstring configured =
      GetString(HKEY_LOCAL_MACHINE, L"Software\\ChenyiOffice", L"EnginePath");
  if (!configured.empty()) {
    std::wstring root = EngineRootFromExe(configured);
    if (!root.empty() && PathExists(configured)) return root;
  }

  std::wstring fresh = FreshEngineRoot();
  if (PathExists(fresh + L"\\program\\soffice.exe")) return fresh;
  return L"";
}

static std::wstring DefaultEnginePath() {
  std::wstring root = CurrentEngineRoot();
  if (root.empty()) return L"";
  std::wstring branded = root + L"\\program\\ChenyiEngine.exe";
  if (PathExists(branded)) return branded;
  return root + L"\\program\\soffice.exe";
}

// An engine that predates this installation is recorded in the registry at
// install time; it is reused, never branded and never uninstalled. The bundled
// payload has its own product identity and install location, so it never
// collides with any other installation on the machine.
static bool DetectForeignEngine() {
  return GetDword(HKEY_LOCAL_MACHINE, L"Software\\ChenyiOffice",
                  L"EnginePreExisting") != 0;
}

static DWORD WINAPI InstallThreadProc(LPVOID);

static void WriteUpdateErrorLog(const std::wstring& message) {
  wchar_t temp[MAX_PATH] = {0};
  GetTempPathW(MAX_PATH, temp);
  std::wstring log = std::wstring(temp) + L"ChenyiOffice-update-error.txt";
  FILE* file = _wfopen(log.c_str(), L"wb");
  if (!file) return;
  std::string text = WideToUtf8(message);
  fwrite(text.data(), 1, text.size(), file);
  fclose(file);
}

static int RunSilentUpdate() {
  // Silent mode is reserved for updates. A fresh install still requires the
  // distribution password in the normal UI.
  if (!EngineAlreadyInstalled()) {
    WriteUpdateErrorLog(
        L"静默模式仅用于已安装用户的更新。未检测到 Chenyi Office，"
        L"首次安装请正常启动安装包并输入安装密码。");
    return 2;
  }

  DWORD threadId = 0;
  HANDLE thread = CreateThread(nullptr, 0, InstallThreadProc, nullptr, 0, &threadId);
  if (!thread) {
    WriteUpdateErrorLog(L"无法创建静默更新线程。");
    return 3;
  }
  WaitForSingleObject(thread, INFINITE);
  CloseHandle(thread);

  if (InterlockedCompareExchange(&g_state, 0, 0) != 2) {
    WriteUpdateErrorLog(g_error.empty() ? L"更新失败" : g_error);
    return 1;
  }
  return 0;
}

static DWORD WINAPI InstallThreadProc(LPVOID) {
  std::wstring temp = TempDirectory();
  std::wstring msi = temp + L"\\ChenyiEngine.msi";

  // Extract the appended MSI payload.
  InterlockedExchange(&g_stage, 1);
  InterlockedExchange(&g_extractPercent, 0);

  HANDLE self = CreateFileW(SelfPath().c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (self == INVALID_HANDLE_VALUE) {
    Fail(L"无法读取安装包自身。");
    return 0;
  }
  LARGE_INTEGER fileSize{};
  GetFileSizeEx(self, &fileSize);
  uint64_t totalSize = (uint64_t)fileSize.QuadPart;
  if (totalSize < sizeof(PayloadFooter) + 1) {
    CloseHandle(self);
    Fail(L"安装包不完整，请重新下载。");
    return 0;
  }

  // The payload footer sits before any Authenticode certificate table.
  // Scan the tail for the magic instead of assuming the last 24 bytes.
  const uint64_t scanSize = std::min<uint64_t>(totalSize, 1024 * 1024);
  std::vector<char> tail((size_t)scanSize);
  LARGE_INTEGER tailPos{};
  tailPos.QuadPart = (LONGLONG)(totalSize - scanSize);
  SetFilePointerEx(self, tailPos, nullptr, FILE_BEGIN);
  DWORD tailRead = 0;
  if (!ReadFile(self, tail.data(), (DWORD)scanSize, &tailRead, nullptr) ||
      tailRead != scanSize) {
    CloseHandle(self);
    Fail(L"安装包读取失败，请重新下载。");
    return 0;
  }

  PayloadFooter footer{};
  bool found = false;
  for (size_t i = 0; i + sizeof(PayloadFooter) <= tail.size(); ++i) {
    if (memcmp(tail.data() + i, "CHNYSET1", 8) != 0) continue;
    PayloadFooter candidate{};
    memcpy(&candidate, tail.data() + i, sizeof(candidate));
    uint64_t footerPos = totalSize - scanSize + i;
    if (candidate.size > 0 && candidate.offset > 0 &&
        candidate.offset + candidate.size == footerPos) {
      footer = candidate;
      found = true;
      break;
    }
  }
  if (!found) {
    CloseHandle(self);
    Fail(L"安装包数据损坏，请重新下载。");
    return 0;
  }

  LARGE_INTEGER payloadPos{};
  payloadPos.QuadPart = (LONGLONG)footer.offset;
  SetFilePointerEx(self, payloadPos, nullptr, FILE_BEGIN);
  HANDLE out = CreateFileW(msi.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
  if (out == INVALID_HANDLE_VALUE) {
    CloseHandle(self);
    Fail(L"无法写入临时文件，请检查磁盘空间。");
    return 0;
  }

  std::vector<BYTE> buffer(1024 * 1024);
  uint64_t remaining = footer.size;
  uint64_t done = 0;
  bool copyOk = true;
  while (remaining > 0) {
    DWORD chunk = (DWORD)std::min<uint64_t>(remaining, buffer.size());
    DWORD got = 0, wrote = 0;
    if (!ReadFile(self, buffer.data(), chunk, &got, nullptr) || got == 0 ||
        !WriteFile(out, buffer.data(), got, &wrote, nullptr) || wrote != got) {
      copyOk = false;
      break;
    }
    remaining -= got;
    done += got;
    InterlockedExchange(&g_extractPercent, (LONG)((done * 100) / footer.size));
  }
  CloseHandle(out);
  CloseHandle(self);
  if (!copyOk) {
    DeleteFileW(msi.c_str());
    Fail(L"解压安装数据失败，请检查磁盘空间。");
    return 0;
  }

  // Install the office engine.
  InterlockedExchange(&g_stage, 2);
  wchar_t programFiles[MAX_PATH] = {0};
  GetEnvironmentVariableW(L"ProgramFiles", programFiles, MAX_PATH);
  bool enginePreExisting = DetectForeignEngine();
  g_enginePreExisting = enginePreExisting;

  // Never run the MSI against an engine the user already had: with the
  // same ProductCode that becomes a repair/upgrade of their installation.
  if (!enginePreExisting) {
    wchar_t systemDir[MAX_PATH] = {0};
    GetSystemDirectoryW(systemDir, MAX_PATH);
    std::wstring msiexec = std::wstring(systemDir) + L"\\msiexec.exe";
    std::wstring installLocation;
    if (EngineAlreadyInstalled()) installLocation = CurrentEngineRoot();
    if (installLocation.empty()) installLocation = FreshEngineRoot();
    std::wstring command = L"\"" + msiexec + L"\" /i \"" + msi +
                           L"\" /qn /norestart ARPSYSTEMCOMPONENT=1 INSTALLLOCATION=\"" +
                           installLocation + L"\"";
    DWORD exitCode = 1;
    if (!RunAndWait(command, exitCode)) {
      Fail(L"无法启动办公内核安装程序。");
      return 0;
    }
    if (exitCode != 0 && exitCode != 3010) {
      wchar_t buf[256];
      swprintf(buf, 256, L"办公内核安装失败（错误代码 %lu）。", exitCode);
      Fail(buf);
      return 0;
    }
    // The bundled payload no longer writes any secondary registry root;
    // Chenyi Office records the engine path under Software\ChenyiOffice.
  }

  // Copy application files.
  InterlockedExchange(&g_stage, 3);
  g_installDir = std::wstring(programFiles) + L"\\ChenyiOffice";
  SHCreateDirectoryExW(nullptr, g_installDir.c_str(), nullptr);

  bool filesOk = WriteResourceFile(102, g_installDir + L"\\ChenyiOffice.exe") &&
                 WriteResourceFile(103, g_installDir + L"\\Uninstall.exe") &&
                 WriteResourceFile(104, g_installDir + L"\\LICENSE") &&
                 WriteResourceFile(105, g_installDir + L"\\NOTICE");
  if (!filesOk) {
    Fail(L"无法写入程序文件，请以管理员身份重试。");
    return 0;
  }

  // Only brand an engine we installed ourselves; a pre-existing engine
  // keeps its original splash and About artwork.
  if (!enginePreExisting) {
    ApplyEngineBranding(CurrentEngineRoot());
  }

  // Shell integration.
  InterlockedExchange(&g_stage, 4);
  wchar_t* publicDesktop = nullptr;
  wchar_t* commonPrograms = nullptr;
  std::wstring desktopLink, startLink, startDir;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_PublicDesktop, 0, nullptr, &publicDesktop))) {
    desktopLink = std::wstring(publicDesktop) + L"\\" + kProduct + L".lnk";
    CoTaskMemFree(publicDesktop);
  }
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_CommonPrograms, 0, nullptr, &commonPrograms))) {
    startDir = std::wstring(commonPrograms) + L"\\" + kProduct;
    SHCreateDirectoryExW(nullptr, startDir.c_str(), nullptr);
    startLink = startDir + L"\\" + kProduct + L".lnk";
    CoTaskMemFree(commonPrograms);
  }

  std::wstring launcher = g_installDir + L"\\ChenyiOffice.exe";
  if (!desktopLink.empty()) CreateShortcut(desktopLink, launcher, kProduct);
  if (!startLink.empty()) CreateShortcut(startLink, launcher, kProduct);

  SetString(HKEY_LOCAL_MACHINE, L"Software\\ChenyiOffice", L"InstallDir", g_installDir);
  SetString(HKEY_LOCAL_MACHINE, L"Software\\ChenyiOffice", L"Version", kVersion);
  SetString(HKEY_LOCAL_MACHINE, L"Software\\ChenyiOffice", L"ProductCode", MSI_PRODUCT_CODE);
  SetDword(HKEY_LOCAL_MACHINE, L"Software\\ChenyiOffice", L"EnginePreExisting",
           enginePreExisting ? 1 : 0);
  std::wstring engine = DefaultEnginePath();
  if (!engine.empty() && GetFileAttributesW(engine.c_str()) != INVALID_FILE_ATTRIBUTES)
    SetString(HKEY_LOCAL_MACHINE, L"Software\\ChenyiOffice", L"EnginePath", engine);

  const wchar_t* uninstallKey =
      L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\ChenyiOffice";
  SetString(HKEY_LOCAL_MACHINE, uninstallKey, L"DisplayName", kProduct);
  SetString(HKEY_LOCAL_MACHINE, uninstallKey, L"DisplayVersion", kVersion);
  SetString(HKEY_LOCAL_MACHINE, uninstallKey, L"Publisher", L"Chenyi Office contributors");
  SetString(HKEY_LOCAL_MACHINE, uninstallKey, L"InstallLocation", g_installDir);
  SetString(HKEY_LOCAL_MACHINE, uninstallKey, L"DisplayIcon", launcher);
  SetString(HKEY_LOCAL_MACHINE, uninstallKey, L"UninstallString",
            L"\"" + g_installDir + L"\\Uninstall.exe\"");
  SetDword(HKEY_LOCAL_MACHINE, uninstallKey, L"EstimatedSize", 1200000);
  SetDword(HKEY_LOCAL_MACHINE, uninstallKey, L"NoModify", 1);
  SetDword(HKEY_LOCAL_MACHINE, uninstallKey, L"NoRepair", 1);

  InterlockedExchange(&g_stage, 5);
  DeleteFileW(msi.c_str());
  InterlockedExchange(&g_state, 2);
  return 0;
}

// ---------------------------------------------------------------------------
// Window
// ---------------------------------------------------------------------------

static void Submit() {
  if (g_screen != WELCOME) return;
  // The password only guards the first installation. Once Chenyi Office is
  // installed, updates (manual or silent) never ask for it again.
  if (!g_updateMode) {
    wchar_t text[64] = {0};
    if (g_edit) GetWindowTextW(g_edit, text, 64);
    if (!PasswordMatches(text)) {
      g_passwordError = true;
      if (g_edit) {
        SetWindowTextW(g_edit, L"");
        SetFocus(g_edit);
      }
      InvalidateRect(g_window, nullptr, FALSE);
      return;
    }
  }
  g_passwordError = false;
  g_screen = INSTALLING;
  if (g_edit) ShowWindow(g_edit, SW_HIDE);
  SetFocus(g_window);
  InterlockedExchange(&g_stage, 0);
  InterlockedExchange(&g_state, 1);
  CreateThread(nullptr, 0, InstallThreadProc, nullptr, 0, nullptr);
  InvalidateRect(g_window, nullptr, FALSE);
}

static void HandleButton() {
  switch (g_screen) {
    case WELCOME:
      Submit();
      break;
    case FINISH: {
      std::wstring launcher = g_installDir + L"\\ChenyiOffice.exe";
      ShellExecuteW(nullptr, L"open", launcher.c_str(), nullptr, g_installDir.c_str(),
                    SW_SHOWNORMAL);
      PostQuitMessage(0);
      break;
    }
    case FAILED:
      g_error.clear();
      g_screen = INSTALLING;
      InterlockedExchange(&g_stage, 0);
      InterlockedExchange(&g_state, 1);
      InterlockedExchange(&g_extractPercent, 0);
      CreateThread(nullptr, 0, InstallThreadProc, nullptr, 0, nullptr);
      InvalidateRect(g_window, nullptr, FALSE);
      break;
    default:
      break;
  }
}

static LRESULT CALLBACK EditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  if (msg == WM_KEYDOWN && wParam == VK_RETURN) {
    Submit();
    return 0;
  }
  if (msg == WM_CHAR && (wParam == L'\r' || wParam == L'\n')) return 0;
  return CallWindowProcW(g_editProc, hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
    case WM_CREATE:
      return 0;

    case WM_DPICHANGED: {
      g_dpi = HIWORD(wParam);
      if (g_dpi <= 0) g_dpi = 96;
      g_width = MulDiv(kBaseWidth, g_dpi, 96);
      g_height = MulDiv(kBaseHeight, g_dpi, 96);
      RECT* suggested = reinterpret_cast<RECT*>(lParam);
      SetWindowPos(hwnd, nullptr, suggested->left, suggested->top, g_width, g_height,
                   SWP_NOZORDER | SWP_NOACTIVATE);
      ui::ApplyWindowChrome(hwnd, g_width, g_height, S(18), false);
      if (g_edit) {
        SetWindowPos(g_edit, nullptr, S(320), S(406), S(300), S(42),
                     SWP_NOZORDER | SWP_NOACTIVATE);
        if (g_editFont) DeleteObject(g_editFont);
        g_editFont =
            CreateFontW(-S(14), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                        DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
        SendMessageW(g_edit, WM_SETFONT, (WPARAM)g_editFont, TRUE);
      }
      InvalidateRect(hwnd, nullptr, FALSE);
      return 0;
    }

    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC dc = BeginPaint(hwnd, &ps);
      g_hits.clear();

      Bitmap buffer(g_width, g_height, PixelFormat32bppPARGB);
      Graphics g(&buffer);
      g.SetSmoothingMode(SmoothingModeAntiAlias);
      g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
      DrawScreen(g);

      Graphics target(dc);
      target.DrawImage(&buffer, 0, 0, g_width, g_height);
      EndPaint(hwnd, &ps);
      return 0;
    }

    case WM_ERASEBKGND:
      return 1;

    case WM_CTLCOLOREDIT: {
      HDC dc = (HDC)wParam;
      SetTextColor(dc, RGB(23, 32, 27));
      SetBkColor(dc, RGB(255, 255, 255));
      static HBRUSH brush = CreateSolidBrush(RGB(255, 255, 255));
      return (LRESULT)brush;
    }

    case WM_TIMER: {
      if (g_screen == INSTALLING) {
        LONG state = InterlockedCompareExchange(&g_state, 0, 0);
        if (state == 2) {
          g_screen = FINISH;
          if (g_edit) ShowWindow(g_edit, SW_HIDE);
          InvalidateRect(hwnd, nullptr, FALSE);
          return 0;
        }
        if (state == 3) {
          g_screen = FAILED;
          if (g_edit) ShowWindow(g_edit, SW_HIDE);
          InvalidateRect(hwnd, nullptr, FALSE);
          return 0;
        }
        LONG m = InterlockedCompareExchange(&g_marquee, 0, 0);
        InterlockedExchange(&g_marquee, m + 8);
        InvalidateRect(hwnd, nullptr, FALSE);
      }
      return 0;
    }

    case WM_MOUSEMOVE: {
      POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
      int hover = HitTest(point);
      if (hover != g_hover) {
        g_hover = hover;
        InvalidateRect(hwnd, nullptr, FALSE);
      }
      TRACKMOUSEEVENT track{};
      track.cbSize = sizeof(track);
      track.dwFlags = TME_LEAVE;
      track.hwndTrack = hwnd;
      TrackMouseEvent(&track);
      return 0;
    }

    case WM_MOUSELEAVE:
      if (g_hover != 0) {
        g_hover = 0;
        InvalidateRect(hwnd, nullptr, FALSE);
      }
      return 0;

    case WM_LBUTTONDOWN: {
      POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
      int action = HitTest(point);
      switch (action) {
        case IA_START:
        case IA_RETRY:
          HandleButton();
          return 0;
        case IA_OPEN:
          HandleButton();
          return 0;
        case IA_EXIT:
          PostQuitMessage(0);
          return 0;
        case IA_MINIMIZE:
          ShowWindow(hwnd, SW_MINIMIZE);
          return 0;
        case IA_CLOSE:
          if (g_screen != INSTALLING) PostQuitMessage(0);
          return 0;
        default:
          break;
      }
      if (point.y < TITLEBAR) {
        ReleaseCapture();
        SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
      }
      return 0;
    }

    case WM_KEYDOWN:
      if (wParam == VK_RETURN && g_screen != INSTALLING) HandleButton();
      return 0;

    case WM_CLOSE:
      if (g_screen == INSTALLING) return 0;
      PostQuitMessage(0);
      return 0;

    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR commandLine, int) {
  g_instance = instance;
  SetProcessDPIAware();
  CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

  HDC screen = GetDC(nullptr);
  g_dpi = screen ? GetDeviceCaps(screen, LOGPIXELSX) : 96;
  if (screen) ReleaseDC(nullptr, screen);
  if (g_dpi <= 0) g_dpi = 96;
  g_width = MulDiv(kBaseWidth, g_dpi, 96);
  g_height = MulDiv(kBaseHeight, g_dpi, 96);

  // Silent mode is the update path used by the launcher: no UI, no password.
  // Parse exact arguments so a path such as "C:\Setup\app.exe" cannot trigger
  // silent mode by accident.
  bool silent = false;
  int argumentCount = 0;
  LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
  if (arguments) {
    for (int i = 1; i < argumentCount; ++i) {
      if (_wcsicmp(arguments[i], L"/S") == 0 || _wcsicmp(arguments[i], L"-s") == 0 ||
          _wcsicmp(arguments[i], L"--silent") == 0) {
        silent = true;
        break;
      }
    }
    LocalFree(arguments);
  }
  if (silent) return RunSilentUpdate();

  ui::InitGdiplus(g_gdiplusToken);

  // Re-running the installer over an existing installation is an update, not a
  // fresh install, so the distribution password is not requested.
  g_updateMode = EngineAlreadyInstalled();
  g_enginePreExisting = DetectForeignEngine();

  g_icon = (HICON)LoadImageW(instance, MAKEINTRESOURCEW(1), IMAGE_ICON, S(64), S(64),
                             LR_DEFAULTCOLOR);

  WNDCLASSW wc{};
  wc.lpfnWndProc = WndProc;
  wc.hInstance = instance;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.lpszClassName = L"ChenyiOfficeSetup";
  wc.hIcon = g_icon;
  RegisterClassW(&wc);

  int x = (GetSystemMetrics(SM_CXSCREEN) - g_width) / 2;
  int y = (GetSystemMetrics(SM_CYSCREEN) - g_height) / 2;
  g_window = CreateWindowExW(0, wc.lpszClassName, kProduct, WS_POPUP, x, y, g_width, g_height,
                             nullptr, nullptr, instance, nullptr);
  if (!g_window) {
    ui::ShutdownGdiplus(g_gdiplusToken);
    return 1;
  }
  ui::ApplyWindowChrome(g_window, g_width, g_height, S(18), false);

  if (!g_updateMode) {
    g_edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                             WS_CHILD | WS_VISIBLE | ES_PASSWORD | ES_AUTOHSCROLL | ES_LEFT,
                             S(320), S(406), S(300), S(42), g_window, (HMENU)1001, instance,
                             nullptr);
    g_editFont =
        CreateFontW(-S(14), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                    DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
    SendMessageW(g_edit, WM_SETFONT, (WPARAM)g_editFont, TRUE);
    SendMessageW(g_edit, EM_SETLIMITTEXT, 64, 0);
    g_editProc = (WNDPROC)SetWindowLongPtrW(g_edit, GWLP_WNDPROC, (LONG_PTR)EditProc);
  }

  ShowWindow(g_window, SW_SHOW);
  UpdateWindow(g_window);
  SetTimer(g_window, 1, 30, nullptr);
  SetFocus(g_edit ? g_edit : g_window);

  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  ui::ShutdownGdiplus(g_gdiplusToken);
  return 0;
}
